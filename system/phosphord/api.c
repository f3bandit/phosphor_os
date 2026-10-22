/* phosphord: the API, one JSON request in, one JSON reply out.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Requests: {"cmd":"...", ...}. Replies: {"ok":true, ...} or {"ok":false,"error":"..."}.
 *   status                                  version, indexing, scan generation, game count
 *   categories                              [{id,name,games}]
 *   systems    [category] [all]             [{id,name,category,games}]; empty ones only with all
 *   games      system [offset] [limit]      {total, games:[{id,title,favorite}]} in natural order
 *   game       id                           details, art (absolute path), play history, files
 *   last                                    the last game played (same shape as game), or null
 *   launch     id                           start it
 *   favorite   id on                        mark or unmark
 *   rescan                                  look for games again
 *   import                                  one-time import of gamelist.xml / media art
 *   link       id base                      id is a hack/mod/translation of base: uses base's art and details
 *   unlink     id                           not based on another game any more
 *   candidates id [q] [limit]               games id could be based on, most alike first: [{id,title,score}]
 *   scrape     [on]                         box art and details: turn on/off; replies on, running,
 *                                           pending, found, not_found, status
 *   rescrape   id                           look this game up again
 *   scrape_sources [order] [off]            the sources in order: [{id,name,on,ready,needs}];
 *                                           with order/off (arrays of ids) saves them */
#include "api.h"
#include "art.h"
#include "cJSON.h"
#include "db.h"
#include "ident.h"
#include "importer.h"
#include "log.h"
#include "indexer.h"
#include "play.h"
#include "scraper.h"
#include "systems.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PHOSD_VERSION "0.4.0"

/* games with at least one file that's here right now */
#define VISIBLE "EXISTS (SELECT 1 FROM files f WHERE f.game_id=g.id AND f.online=1)"

static char *reply(cJSON *o)
{
	char *s = cJSON_PrintUnformatted(o);
	cJSON_Delete(o);
	return s;
}

static char *fail(const char *msg)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddBoolToObject(o, "ok", 0);
	cJSON_AddStringToObject(o, "error", msg);
	return reply(o);
}

static cJSON *ok(void)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddBoolToObject(o, "ok", 1);
	return o;
}

static void add_text(cJSON *o, const char *k, sqlite3_stmt *st, int col)
{
	const unsigned char *t = sqlite3_column_text(st, col);
	if (t) cJSON_AddStringToObject(o, k, (const char *)t);
	else cJSON_AddNullToObject(o, k);
}

static const char *where(const char *root)
{
	if (!strncmp(root, "/media/usb", 10)) return "USB / NVMe drive";
	if (strstr(root, "/cifs/")) return "Network share";
	if (strstr(root, "/_Arcade")) return "SD card";
	return "SD card";
}

/* the scraper's view of a game: whether it should look it up, and why not */
static const char *scrape_state(sqlite3_stmt *st, int based_col, int key_col, int scrape_col)
{
	if (sqlite3_column_type(st, based_col) != SQLITE_NULL) return "linked";      /* uses its base's art */
	const char *key = (const char *)sqlite3_column_text(st, key_col);
	const char *sc = (const char *)sqlite3_column_text(st, scrape_col);
	if (sc && !strcmp(sc, "miss")) return "not_found";                           /* looked up, unknown: no art */
	if (sc && !strcmp(sc, "done")) return "found";
	if (!key || !strncmp(key, "n:", 2)) return "unidentified";                   /* not hashed (yet), or a .chd */
	return "ready";
}

/* details: the game's own, or (when it's based on another game) the base's where it has none */
static cJSON *game_json(sqlite3 *db, long long id)
{
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db,
	        "SELECT g.id, g.system_id, g.title,"
	        " IFNULL(g.year, b.year), IFNULL(g.developer, b.developer), IFNULL(g.publisher, b.publisher),"
	        " IFNULL(g.genre, b.genre), IFNULL(g.players, b.players), IFNULL(g.description, b.description),"
	        " IFNULL(g.art_box, b.art_box),"
	        " g.favorite, g.play_count, g.play_seconds, g.last_played, IFNULL(g.region, b.region),"
	        " g.based_on, b.title, g.key, g.scrape, IFNULL(g.ref_name, b.ref_name)"
	        " FROM games g LEFT JOIN games b ON b.id = g.based_on WHERE g.id=?", -1, &st, NULL) != SQLITE_OK)
		return NULL;
	sqlite3_bind_int64(st, 1, id);
	if (sqlite3_step(st) != SQLITE_ROW) { sqlite3_finalize(st); return NULL; }
	cJSON *g = cJSON_CreateObject();
	cJSON_AddNumberToObject(g, "id", (double)sqlite3_column_int64(st, 0));
	add_text(g, "system", st, 1);
	const gsys_t *s = system_by_id((const char *)sqlite3_column_text(st, 1));
	cJSON_AddStringToObject(g, "system_name", s ? s->name : "");
	cJSON_AddNumberToObject(g, "category", s ? s->cat : 0);
	add_text(g, "title", st, 2);
	add_text(g, "year", st, 3);
	add_text(g, "developer", st, 4);
	add_text(g, "publisher", st, 5);
	add_text(g, "genre", st, 6);
	add_text(g, "players", st, 7);
	add_text(g, "description", st, 8);
	if (sqlite3_column_text(st, 9)) {
		char abs[1100];
		art_path((const char *)sqlite3_column_text(st, 9), abs, sizeof(abs));
		cJSON_AddStringToObject(g, "art", abs);
	} else {
		cJSON_AddNullToObject(g, "art");
	}
	cJSON_AddBoolToObject(g, "favorite", sqlite3_column_int(st, 10));
	cJSON_AddNumberToObject(g, "play_count", sqlite3_column_int(st, 11));
	cJSON_AddNumberToObject(g, "play_seconds", (double)sqlite3_column_int64(st, 12));
	if (sqlite3_column_type(st, 13) != SQLITE_NULL) cJSON_AddNumberToObject(g, "last_played", (double)sqlite3_column_int64(st, 13));
	else cJSON_AddNullToObject(g, "last_played");
	add_text(g, "region", st, 14);
	if (sqlite3_column_type(st, 15) != SQLITE_NULL) {
		cJSON *b = cJSON_AddObjectToObject(g, "based_on");
		cJSON_AddNumberToObject(b, "id", (double)sqlite3_column_int64(st, 15));
		add_text(b, "title", st, 16);
	} else {
		cJSON_AddNullToObject(g, "based_on");
	}
	cJSON_AddStringToObject(g, "scrape", scrape_state(st, 15, 17, 18));
	add_text(g, "official_name", st, 19);
	sqlite3_finalize(st);

	cJSON *files = cJSON_AddArrayToObject(g, "files");
	if (sqlite3_prepare_v2(db, "SELECT path, root, size, online, crc32, serial FROM files WHERE game_id=? ORDER BY online DESC, id", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(st, 1, id);
		while (sqlite3_step(st) == SQLITE_ROW) {
			cJSON *f = cJSON_CreateObject();
			add_text(f, "path", st, 0);
			cJSON_AddStringToObject(f, "where", where((const char *)sqlite3_column_text(st, 1)));
			cJSON_AddNumberToObject(f, "size", (double)sqlite3_column_int64(st, 2));
			cJSON_AddBoolToObject(f, "online", sqlite3_column_int(st, 3));
			add_text(f, "crc32", st, 4);
			add_text(f, "serial", st, 5);
			cJSON_AddItemToArray(files, f);
		}
		sqlite3_finalize(st);
	}
	return g;
}

static char *cmd_status(sqlite3 *db)
{
	cJSON *o = ok();
	cJSON_AddStringToObject(o, "version", PHOSD_VERSION);
	cJSON_AddBoolToObject(o, "indexing", indexer_busy());
	cJSON_AddBoolToObject(o, "identifying", indexer_identifying());
	cJSON_AddNumberToObject(o, "generation", indexer_generation());
	sqlite3_stmt *st;
	int n = 0;
	if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM games g WHERE " VISIBLE, -1, &st, NULL) == SQLITE_OK) {
		if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int(st, 0);
		sqlite3_finalize(st);
	}
	cJSON_AddNumberToObject(o, "games", n);
	return reply(o);
}

static int system_count(sqlite3 *db, const char *id)
{
	sqlite3_stmt *st;
	int n = 0;
	if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM games g WHERE g.system_id=? AND " VISIBLE, -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_text(st, 1, id, -1, SQLITE_STATIC);
		if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int(st, 0);
		sqlite3_finalize(st);
	}
	return n;
}

static char *cmd_categories(sqlite3 *db)
{
	cJSON *o = ok(), *arr = cJSON_AddArrayToObject(o, "categories");
	for (int c = 0; c < CAT_COUNT; c++) {
		int n = 0;
		for (int i = 0; i < NSYS; i++) if (SYS[i].cat == (category_t)c) n += system_count(db, SYS[i].id);
		cJSON *x = cJSON_CreateObject();
		cJSON_AddNumberToObject(x, "id", c);
		cJSON_AddStringToObject(x, "name", category_name((category_t)c));
		cJSON_AddNumberToObject(x, "games", n);
		cJSON_AddItemToArray(arr, x);
	}
	return reply(o);
}

static char *cmd_systems(sqlite3 *db, cJSON *req)
{
	cJSON *cat = cJSON_GetObjectItem(req, "category");
	int all = cJSON_IsTrue(cJSON_GetObjectItem(req, "all"));
	cJSON *o = ok(), *arr = cJSON_AddArrayToObject(o, "systems");
	for (int i = 0; i < NSYS; i++) {
		if (cJSON_IsNumber(cat) && SYS[i].cat != (category_t)cat->valueint) continue;
		int n = system_count(db, SYS[i].id);
		if (!n && !all) continue;
		char rbf[96];
		cJSON *x = cJSON_CreateObject();
		cJSON_AddStringToObject(x, "id", SYS[i].id);
		cJSON_AddStringToObject(x, "name", SYS[i].name);
		cJSON_AddNumberToObject(x, "category", SYS[i].cat);
		cJSON_AddNumberToObject(x, "games", n);
		cJSON_AddBoolToObject(x, "core", system_core(&SYS[i], rbf, sizeof(rbf)));
		cJSON_AddItemToArray(arr, x);
	}
	return reply(o);
}

static char *cmd_games(sqlite3 *db, cJSON *req)
{
	cJSON *sys = cJSON_GetObjectItem(req, "system");
	if (!cJSON_IsString(sys)) return fail("system missing");
	cJSON *off = cJSON_GetObjectItem(req, "offset"), *lim = cJSON_GetObjectItem(req, "limit");
	int offset = cJSON_IsNumber(off) ? off->valueint : 0, limit = cJSON_IsNumber(lim) ? lim->valueint : 100000;
	cJSON *o = ok();
	cJSON_AddNumberToObject(o, "total", system_count(db, sys->valuestring));
	cJSON *arr = cJSON_AddArrayToObject(o, "games");
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, "SELECT g.id, g.title, g.favorite, g.based_on FROM games g WHERE g.system_id=? AND " VISIBLE
	                           " ORDER BY g.title COLLATE NATSORT LIMIT ? OFFSET ?", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_text(st, 1, sys->valuestring, -1, SQLITE_STATIC);
		sqlite3_bind_int(st, 2, limit);
		sqlite3_bind_int(st, 3, offset);
		while (sqlite3_step(st) == SQLITE_ROW) {
			cJSON *x = cJSON_CreateObject();
			cJSON_AddNumberToObject(x, "id", (double)sqlite3_column_int64(st, 0));
			add_text(x, "title", st, 1);
			cJSON_AddBoolToObject(x, "favorite", sqlite3_column_int(st, 2));
			cJSON_AddBoolToObject(x, "linked", sqlite3_column_type(st, 3) != SQLITE_NULL);
			cJSON_AddItemToArray(arr, x);
		}
		sqlite3_finalize(st);
	} else {
		plog("games query: %s", sqlite3_errmsg(db));
	}
	return reply(o);
}

static long long req_id(cJSON *req)
{
	cJSON *id = cJSON_GetObjectItem(req, "id");
	return cJSON_IsNumber(id) ? (long long)id->valuedouble : -1;
}

static char *cmd_game(sqlite3 *db, long long id)
{
	cJSON *g = id >= 0 ? game_json(db, id) : NULL;
	if (!g) return fail("no such game");
	cJSON *o = ok();
	cJSON_AddItemToObject(o, "game", g);
	return reply(o);
}

static char *cmd_last(sqlite3 *db)
{
	char buf[32];
	db_meta_get(db, "last_game", buf, sizeof(buf));
	cJSON *o = ok();
	cJSON *g = buf[0] ? game_json(db, atoll(buf)) : NULL;
	if (g) cJSON_AddItemToObject(o, "game", g);
	else cJSON_AddNullToObject(o, "game");
	return reply(o);
}

/* ---------- "based on" ---------- */

static int game_system(sqlite3 *db, long long id, char *sys, int n, long long *based_on)
{
	sqlite3_stmt *st;
	int found = 0;
	if (sqlite3_prepare_v2(db, "SELECT system_id, based_on FROM games WHERE id=?", -1, &st, NULL) != SQLITE_OK) return 0;
	sqlite3_bind_int64(st, 1, id);
	if (sqlite3_step(st) == SQLITE_ROW) {
		snprintf(sys, (size_t)n, "%s", sqlite3_column_text(st, 0));
		if (based_on) *based_on = sqlite3_column_type(st, 1) == SQLITE_NULL ? -1 : sqlite3_column_int64(st, 1);
		found = 1;
	}
	sqlite3_finalize(st);
	return found;
}

static char *cmd_link(sqlite3 *db, cJSON *req)
{
	long long id = req_id(req), base = -1, bb = -1;
	cJSON *b = cJSON_GetObjectItem(req, "base");
	if (cJSON_IsNumber(b)) base = (long long)b->valuedouble;
	char s1[32], s2[32];
	if (!game_system(db, id, s1, sizeof(s1), NULL)) return fail("no such game");
	if (!game_system(db, base, s2, sizeof(s2), &bb)) return fail("no such base game");
	if (strcmp(s1, s2)) return fail("the base game has to be for the same system");
	if (bb >= 0) base = bb;                          /* based on a hack: use what that hack is based on */
	if (base == id) return fail("a game can't be based on itself");
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, "UPDATE games SET based_on=?2 WHERE id=?1", -1, &st, NULL) != SQLITE_OK) return fail(sqlite3_errmsg(db));
	sqlite3_bind_int64(st, 1, id);
	sqlite3_bind_int64(st, 2, base);
	sqlite3_step(st);
	sqlite3_finalize(st);
	/* games based on this one now point at its base too (no chains) */
	if (sqlite3_prepare_v2(db, "UPDATE games SET based_on=?2 WHERE based_on=?1", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(st, 1, id);
		sqlite3_bind_int64(st, 2, base);
		sqlite3_step(st);
		sqlite3_finalize(st);
	}
	cJSON *g = game_json(db, id), *o = ok();
	if (g) cJSON_AddItemToObject(o, "game", g);
	return reply(o);
}

static char *cmd_unlink(sqlite3 *db, cJSON *req)
{
	long long id = req_id(req);
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, "UPDATE games SET based_on=NULL WHERE id=?", -1, &st, NULL) != SQLITE_OK) return fail(sqlite3_errmsg(db));
	sqlite3_bind_int64(st, 1, id);
	sqlite3_step(st);
	sqlite3_finalize(st);
	cJSON *g = game_json(db, id);
	if (!g) return fail("no such game");
	cJSON *o = ok();
	cJSON_AddItemToObject(o, "game", g);
	return reply(o);
}

typedef struct { long long id; int score; char *title; } cand_t;

static int cand_cmp(const void *a, const void *b)
{
	const cand_t *x = a, *y = b;
	if (x->score != y->score) return y->score - x->score;
	return natcmp(x->title, y->title);
}

/* games this one could be based on: same system, not itself, not hacks themselves, most alike first */
static char *cmd_candidates(sqlite3 *db, cJSON *req)
{
	long long id = req_id(req), cur_base = -1;
	char sys[32], me[512] = "", mine[512], q[512] = "";
	if (!game_system(db, id, sys, sizeof(sys), &cur_base)) return fail("no such game");
	cJSON *lim = cJSON_GetObjectItem(req, "limit"), *qq = cJSON_GetObjectItem(req, "q");
	int limit = cJSON_IsNumber(lim) ? lim->valueint : 200;
	if (cJSON_IsString(qq)) title_norm(qq->valuestring, q, sizeof(q));
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, "SELECT title FROM games WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(st, 1, id);
		if (sqlite3_step(st) == SQLITE_ROW) snprintf(me, sizeof(me), "%s", sqlite3_column_text(st, 0));
		sqlite3_finalize(st);
	}
	title_norm(me, mine, sizeof(mine));
	cand_t *c = NULL;
	int n = 0, cap = 0;
	if (sqlite3_prepare_v2(db, "SELECT g.id, g.title FROM games g WHERE g.system_id=? AND g.id<>? AND g.based_on IS NULL", -1, &st, NULL) != SQLITE_OK)
		return fail(sqlite3_errmsg(db));
	sqlite3_bind_text(st, 1, sys, -1, SQLITE_STATIC);
	sqlite3_bind_int64(st, 2, id);
	while (sqlite3_step(st) == SQLITE_ROW) {
		const char *t = (const char *)sqlite3_column_text(st, 1);
		char norm[512];
		title_norm(t, norm, sizeof(norm));
		if (q[0] && !strstr(norm, q)) continue;
		if (n == cap) { cap = cap ? cap * 2 : 256; c = realloc(c, sizeof(*c) * (size_t)cap); }
		c[n].id = sqlite3_column_int64(st, 0);
		c[n].score = title_similarity(q[0] ? q : mine, norm);
		c[n].title = strdup(t);
		n++;
	}
	sqlite3_finalize(st);
	qsort(c, (size_t)n, sizeof(*c), cand_cmp);
	cJSON *o = ok();
	cJSON_AddNumberToObject(o, "total", n);
	if (cur_base >= 0) cJSON_AddNumberToObject(o, "current", (double)cur_base);
	else cJSON_AddNullToObject(o, "current");
	cJSON *arr = cJSON_AddArrayToObject(o, "games");
	for (int i = 0; i < n; i++) {
		if (i < limit) {
			cJSON *x = cJSON_CreateObject();
			cJSON_AddNumberToObject(x, "id", (double)c[i].id);
			cJSON_AddStringToObject(x, "title", c[i].title);
			cJSON_AddNumberToObject(x, "score", c[i].score);
			cJSON_AddItemToArray(arr, x);
		}
		free(c[i].title);
	}
	free(c);
	return reply(o);
}

char *api_handle(sqlite3 *db, const char *request)
{
	cJSON *req = cJSON_Parse(request);
	if (!req) return fail("not JSON");
	cJSON *c = cJSON_GetObjectItem(req, "cmd");
	char *out;
	if (!cJSON_IsString(c)) out = fail("cmd missing");
	else if (!strcmp(c->valuestring, "status")) out = cmd_status(db);
	else if (!strcmp(c->valuestring, "categories")) out = cmd_categories(db);
	else if (!strcmp(c->valuestring, "systems")) out = cmd_systems(db, req);
	else if (!strcmp(c->valuestring, "games")) out = cmd_games(db, req);
	else if (!strcmp(c->valuestring, "game")) out = cmd_game(db, req_id(req));
	else if (!strcmp(c->valuestring, "last")) out = cmd_last(db);
	else if (!strcmp(c->valuestring, "link")) out = cmd_link(db, req);
	else if (!strcmp(c->valuestring, "unlink")) out = cmd_unlink(db, req);
	else if (!strcmp(c->valuestring, "candidates")) out = cmd_candidates(db, req);
	else if (!strcmp(c->valuestring, "scrape")) {
		cJSON *on = cJSON_GetObjectItem(req, "on");
		if (cJSON_IsBool(on)) scraper_set(db, cJSON_IsTrue(on));
		scrape_info_t si;
		scraper_info(db, &si);
		cJSON *o = ok();
		cJSON_AddBoolToObject(o, "on", si.on);
		cJSON_AddBoolToObject(o, "running", si.running);
		cJSON_AddNumberToObject(o, "pending", si.pending);
		cJSON_AddNumberToObject(o, "found", si.done);
		cJSON_AddNumberToObject(o, "not_found", si.missing);
		cJSON_AddStringToObject(o, "status", si.status);
		out = reply(o);
	} else if (!strcmp(c->valuestring, "scrape_sources")) {
		cJSON *ord = cJSON_GetObjectItem(req, "order"), *off = cJSON_GetObjectItem(req, "off");
		if (cJSON_IsArray(ord) || cJSON_IsArray(off)) {
			char o[400] = "", f[400] = "";
			scrape_source_t cur[16];
			int n = scraper_sources(cur, 16);
			cJSON *x;
			if (cJSON_IsArray(ord)) {
				cJSON_ArrayForEach(x, ord) if (cJSON_IsString(x)) {
					if (o[0]) strncat(o, ", ", sizeof(o) - strlen(o) - 1);
					strncat(o, x->valuestring, sizeof(o) - strlen(o) - 1);
				}
			} else {
				for (int i = 0; i < n; i++) { if (o[0]) strncat(o, ", ", sizeof(o) - strlen(o) - 1); strncat(o, cur[i].id, sizeof(o) - strlen(o) - 1); }
			}
			if (cJSON_IsArray(off)) {
				cJSON_ArrayForEach(x, off) if (cJSON_IsString(x)) {
					if (f[0]) strncat(f, ", ", sizeof(f) - strlen(f) - 1);
					strncat(f, x->valuestring, sizeof(f) - strlen(f) - 1);
				}
			} else {
				for (int i = 0; i < n; i++) if (!cur[i].on) { if (f[0]) strncat(f, ", ", sizeof(f) - strlen(f) - 1); strncat(f, cur[i].id, sizeof(f) - strlen(f) - 1); }
			}
			if (scraper_set_sources(db, o, f)) { cJSON_Delete(req); return fail("can't save scraper.ini"); }
		}
		scrape_source_t ss[16];
		int n = scraper_sources(ss, 16);
		cJSON *o = ok(), *arr = cJSON_AddArrayToObject(o, "sources");
		for (int i = 0; i < n; i++) {
			cJSON *x = cJSON_CreateObject();
			cJSON_AddStringToObject(x, "id", ss[i].id);
			cJSON_AddStringToObject(x, "name", ss[i].name);
			cJSON_AddBoolToObject(x, "on", ss[i].on);
			cJSON_AddBoolToObject(x, "ready", ss[i].ready);
			if (ss[i].needs) cJSON_AddStringToObject(x, "needs", ss[i].needs);
			else cJSON_AddNullToObject(x, "needs");
			cJSON_AddItemToArray(arr, x);
		}
		out = reply(o);
	} else if (!strcmp(c->valuestring, "rescrape")) {
		out = scraper_retry(db, req_id(req)) == 0 ? reply(ok()) : fail("no such game");
	}
	else if (!strcmp(c->valuestring, "launch")) {
		char err[300];
		out = play_launch(db, req_id(req), err, sizeof(err)) == 0 ? reply(ok()) : fail(err);
	} else if (!strcmp(c->valuestring, "favorite")) {
		sqlite3_stmt *st;
		int on = cJSON_IsTrue(cJSON_GetObjectItem(req, "on"));
		if (sqlite3_prepare_v2(db, "UPDATE games SET favorite=? WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
			sqlite3_bind_int(st, 1, on);
			sqlite3_bind_int64(st, 2, req_id(req));
			sqlite3_step(st);
			sqlite3_finalize(st);
		}
		out = reply(ok());
	} else if (!strcmp(c->valuestring, "rescan")) {
		indexer_request();
		out = reply(ok());
	} else if (!strcmp(c->valuestring, "import")) {
		import_result_t r = import_existing(db);
		cJSON *o = ok();
		cJSON_AddNumberToObject(o, "games", r.games);
		cJSON_AddNumberToObject(o, "descriptions", r.details);
		cJSON_AddNumberToObject(o, "art", r.art);
		out = reply(o);
	} else out = fail("unknown cmd");
	cJSON_Delete(req);
	return out;
}
