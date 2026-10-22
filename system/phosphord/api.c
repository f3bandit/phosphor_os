/* phosphord: the API, one JSON request in, one JSON reply out.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Requests: {"cmd":"...", ...}. Replies: {"ok":true, ...} or {"ok":false,"error":"..."}.
 *   status                                  version, indexing, scan generation, game count
 *   categories                              [{id,name,games}]
 *   systems    [category] [all]             [{id,name,category,games}]; empty ones only with all
 *   games      [system] [filter] [q] [offset] [limit]
 *                                           {total, games:[{id,title,favorite,linked,system,art,details,scrape}]};
 *                                           filter: missing_art|missing_details|not_found|linked|unidentified|favorites|recent
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
 *   wifi       [do: scan|connect|forget|up|down|undo] [ssid] [password] [hidden]
 *                                           {status{...}, scanning, networks:[{ssid, signal, bands,
 *                                           secure, saved}], saved:[{ssid, open}]}; passwords are
 *                                           never sent back
 *   video      [system] [set{what:value}] [preset] [undo]
 *                                           screen filters per system: no system = every system with
 *                                           a core and its settings; changes show next time it starts
 *   video_files kind                        filters | masks | gamma | presets: [relative paths]
 *   diag       [run] [bundle]               whole-system checks (diag module), in the background:
 *                                           {installed, busy, at, summary{level,text}, results:[{area,
 *                                           key, level, message}], bundle}; run/bundle start them
 *   scrape_sources [order] [off]            the sources in order: [{id,name,on,ready,needs}];
 *                                           with order/off (arrays of ids) saves them
 *   edit       id set{title, description, year, developer, publisher, genre, players, region}
 *                                           change a game's details ("" or null clears; title can't be empty)
 *   set_art    id file                      store an image file as the game's box art (scaled, JPEG)
 *   cloud      [do: setup|add|use|test|push|preview|pull|devices|history] [from] [name] [type]
 *              [settings{}] [folder] [force]
 *                                           cloud saves (cloud-sync module): {installed, status{...},
 *                                           remotes:[{name,type,in_use}], job{name,busy,ok,output,at}}
 *   backup     [do: now|preview|restore] [name] [only: "saves|savestates"]
 *                                           snapshots (backup-agent module): {installed, status{...},
 *                                           snapshots:[{name, kb, when, reason}], job{name,busy,ok,output,at}}
 *   cores      [do: update]                cores, filters and the rest with MiSTer's own downloader
 *                                           (Scripts/update.sh), only when asked: {available, busy,
 *                                           worked, at, line}
 *   samba      [on: true|false]            MiSTer's file sharing (Samba): turn on / off; available, on, running
 *   bluetooth  [do: pair|cancel|forget|disconnect] [mac]
 *                                           {present, devices:[{mac, name, connected, input, icon}],
 *                                           pairing{busy, ok, at, found, message}}; pair looks for a
 *                                           controller in pairing mode (MiSTer's btctl), up to 90 s
 *   ports      [prepare]                   the front ports (phosphor-main + the Phosphor menu core):
 *                                           {main_api, available, can_load, in_game, on}; prepare loads
 *                                           the Phosphor menu core if needed (menu only) and turns them on,
 *                                           in the background (same as memcard do:prepare)
 *   pads                                    both controllers: {on, pads:[{port, connected, id, buttons,
 *                                           rx, ry, lx, ly}]} (turns the ports on)
 *   pad_analog port                         switch that DualShock to analog
 *   memcard    [do: backup|restore|check|prepare] [port] [name]
 *                                           memory cards: {ports{...}, job{name, port, busy, ok, done,
 *                                           total, message, file}, backups:[{name, bytes, at}]};
 *                                           restore backs the card up first
 *   remote     [on] [do: pair|stop|forget] [id]
 *                                           remote access (local socket only): {on, port, listening,
 *                                           error, addresses, pairing{code, seconds}|null, paired,
 *                                           devices:[{id, name, created, last_seen}]} */
#include "api.h"
#include "art.h"
#include "cJSON.h"
#include "db.h"
#include "diag.h"
#include "wifi.h"
#include "video.h"
#include "remote.h"
#include "cloud.h"
#include "backup.h"
#include "memcard.h"
#include "bt.h"
#include "cores.h"
#include "samba.h"
#include "version.h"
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

/* games [system] [filter] [q] [offset] [limit]: a system's games, or (with filter or q) games anywhere.
 * filter: missing_art | missing_details | not_found | linked | unidentified | favorites | recent
 * (recent: played ones, last played first) */
static char *cmd_games(sqlite3 *db, cJSON *req)
{
	const char *sys = cJSON_GetStringValue(cJSON_GetObjectItem(req, "system"));
	const char *filter = cJSON_GetStringValue(cJSON_GetObjectItem(req, "filter"));
	const char *q = cJSON_GetStringValue(cJSON_GetObjectItem(req, "q"));
	if (filter && !*filter) filter = NULL;
	if (q && !*q) q = NULL;
	if (!sys && !filter && !q) return fail("system missing");
	cJSON *off = cJSON_GetObjectItem(req, "offset"), *lim = cJSON_GetObjectItem(req, "limit");
	int offset = cJSON_IsNumber(off) ? off->valueint : 0, limit = cJSON_IsNumber(lim) ? lim->valueint : 100000;
	const char *fw = "";
	if (filter) {
		if (!strcmp(filter, "missing_art")) fw = " AND COALESCE(g.art_box, b.art_box) IS NULL";
		else if (!strcmp(filter, "missing_details")) fw = " AND COALESCE(g.description, b.description) IS NULL";
		else if (!strcmp(filter, "not_found")) fw = " AND g.scrape='miss' AND g.based_on IS NULL";
		else if (!strcmp(filter, "linked")) fw = " AND g.based_on IS NOT NULL";
		else if (!strcmp(filter, "unidentified")) fw = " AND g.key LIKE 'n:%'";
		else if (!strcmp(filter, "favorites")) fw = " AND g.favorite=1";
		else if (!strcmp(filter, "recent")) fw = " AND g.last_played IS NOT NULL";
		else return fail("filter: missing_art, missing_details, not_found, linked, unidentified, favorites or recent");
	}
	char where[600];
	snprintf(where, sizeof(where), " FROM games g LEFT JOIN games b ON b.id=g.based_on WHERE " VISIBLE "%s%s%s",
	         sys ? " AND g.system_id=?1" : "", fw, q ? " AND g.title LIKE ?2 ESCAPE '\\'" : "");
	char like[300] = "";
	if (q) {
		size_t k = 0;
		like[k++] = '%';
		for (const char *c = q; *c && k < sizeof(like) - 3; c++) {
			if (*c == '%' || *c == '_' || *c == '\\') like[k++] = '\\';
			like[k++] = *c;
		}
		like[k++] = '%';
		like[k] = 0;
	}
	cJSON *o = ok();
	sqlite3_stmt *st;
	char sql[1200];
	int total = 0;
	snprintf(sql, sizeof(sql), "SELECT COUNT(*)%s", where);
	if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
		if (sys) sqlite3_bind_text(st, 1, sys, -1, SQLITE_STATIC);
		if (q) sqlite3_bind_text(st, 2, like, -1, SQLITE_STATIC);
		if (sqlite3_step(st) == SQLITE_ROW) total = sqlite3_column_int(st, 0);
		sqlite3_finalize(st);
	}
	cJSON_AddNumberToObject(o, "total", total);
	cJSON *arr = cJSON_AddArrayToObject(o, "games");
	snprintf(sql, sizeof(sql), "SELECT g.id, g.title, g.favorite, g.based_on, g.system_id,"
	         " COALESCE(g.art_box, b.art_box) IS NOT NULL, COALESCE(g.description, b.description) IS NOT NULL,"
	         " g.key, g.scrape%s ORDER BY %sg.title COLLATE NATSORT LIMIT ?3 OFFSET ?4", where,
	         (filter && !strcmp(filter, "recent")) ? "g.last_played DESC, " : (sys || (filter && !strcmp(filter, "favorites")) || q) ? "" : "g.system_id, ");
	if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
		if (sys) sqlite3_bind_text(st, 1, sys, -1, SQLITE_STATIC);
		if (q) sqlite3_bind_text(st, 2, like, -1, SQLITE_STATIC);
		sqlite3_bind_int(st, 3, limit);
		sqlite3_bind_int(st, 4, offset);
		while (sqlite3_step(st) == SQLITE_ROW) {
			cJSON *x = cJSON_CreateObject();
			cJSON_AddNumberToObject(x, "id", (double)sqlite3_column_int64(st, 0));
			add_text(x, "title", st, 1);
			cJSON_AddBoolToObject(x, "favorite", sqlite3_column_int(st, 2));
			cJSON_AddBoolToObject(x, "linked", sqlite3_column_type(st, 3) != SQLITE_NULL);
			add_text(x, "system", st, 4);
			cJSON_AddBoolToObject(x, "art", sqlite3_column_int(st, 5));
			cJSON_AddBoolToObject(x, "details", sqlite3_column_int(st, 6));
			cJSON_AddStringToObject(x, "scrape", scrape_state(st, 3, 7, 8));
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

static char *cmd_wifi(cJSON *req)
{
	const char *act = cJSON_GetStringValue(cJSON_GetObjectItem(req, "do"));
	const char *ssid = cJSON_GetStringValue(cJSON_GetObjectItem(req, "ssid"));
	const char *pass = cJSON_GetStringValue(cJSON_GetObjectItem(req, "password"));
	int hidden = cJSON_IsTrue(cJSON_GetObjectItem(req, "hidden"));
	char err[200] = "";
	int rc = 0;
	if (act && !strcmp(act, "scan")) wifi_scan_start();
	else if (act && !strcmp(act, "connect")) rc = wifi_connect(ssid, pass, hidden, err, sizeof(err));
	else if (act && !strcmp(act, "forget")) rc = wifi_forget(ssid ? ssid : "", err, sizeof(err));
	else if (act && (!strcmp(act, "up") || !strcmp(act, "down"))) rc = wifi_move(ssid ? ssid : "", !strcmp(act, "up") ? -1 : 1, err, sizeof(err));
	else if (act && !strcmp(act, "undo")) rc = wifi_undo(err, sizeof(err));
	else if (act) return fail("unknown wifi action");
	if (rc) return fail(err);

	wifi_status_t st;
	wifi_status(&st);
	cJSON *o = ok(), *s = cJSON_AddObjectToObject(o, "status");
	cJSON_AddStringToObject(s, "iface", st.iface);
	cJSON_AddBoolToObject(s, "present", st.present);
	cJSON_AddBoolToObject(s, "connected", st.connected);
	cJSON_AddStringToObject(s, "ssid", st.ssid);
	cJSON_AddNumberToObject(s, "signal", st.signal);
	cJSON_AddNumberToObject(s, "freq", st.freq);
	cJSON_AddStringToObject(s, "ip", st.ip);
	cJSON_AddBoolToObject(s, "supplicant", st.supplicant);
	wifi_saved_t sv[WIFI_SAVED_MAX];
	int ns = wifi_saved(sv, WIFI_SAVED_MAX);
	cJSON *sa = cJSON_AddArrayToObject(o, "saved");
	for (int i = 0; i < ns; i++) {
		cJSON *x = cJSON_CreateObject();
		cJSON_AddStringToObject(x, "ssid", sv[i].ssid);
		cJSON_AddBoolToObject(x, "open", sv[i].open);
		cJSON_AddItemToArray(sa, x);
	}
	wifi_net_t nets[WIFI_MAX];
	int busy = 0;
	time_t at = 0;
	char serr[160];
	int nn = wifi_scan_results(nets, WIFI_MAX, &busy, &at, serr, sizeof(serr));
	cJSON_AddBoolToObject(o, "scanning", busy);
	if (at) cJSON_AddNumberToObject(o, "scanned_at", (double)at);
	else cJSON_AddNullToObject(o, "scanned_at");
	if (serr[0]) cJSON_AddStringToObject(o, "scan_error", serr);
	cJSON *na = cJSON_AddArrayToObject(o, "networks");
	for (int i = 0; i < nn; i++) {
		cJSON *x = cJSON_CreateObject();
		cJSON_AddStringToObject(x, "ssid", nets[i].ssid);
		cJSON_AddNumberToObject(x, "signal", nets[i].signal);
		cJSON_AddNumberToObject(x, "bands", nets[i].bands);
		cJSON_AddBoolToObject(x, "secure", nets[i].secure);
		cJSON_AddStringToObject(x, "security", nets[i].enterprise ? "enterprise" : nets[i].wpa3 && !nets[i].psk ? "wpa3" : nets[i].secure ? "wpa" : "open");
		int saved = 0;
		for (int k = 0; k < ns; k++) if (!strcmp(sv[k].ssid, nets[i].ssid)) saved = 1;
		cJSON_AddBoolToObject(x, "saved", saved);
		cJSON_AddBoolToObject(x, "current", st.connected && !strcmp(st.ssid, nets[i].ssid));
		cJSON_AddItemToArray(na, x);
	}
	return reply(o);
}

static cJSON *video_json(const char *system_id, const char *core)
{
	video_state_t v;
	video_get(core, &v);
	cJSON *x = cJSON_CreateObject();
	cJSON_AddStringToObject(x, "system", system_id);
	const gsys_t *s = system_by_id(system_id);
	cJSON_AddStringToObject(x, "name", s ? s->name : system_id);
	cJSON_AddStringToObject(x, "core", core);
	cJSON_AddBoolToObject(x, "custom", v.custom);
	static const char *K[4] = { "filter", "vertical", "scanlines", "interlace" };
	for (int i = 0; i < 4; i++) {
		if (v.flt_on[i] && v.flt[i][0]) cJSON_AddStringToObject(x, K[i], v.flt[i]);
		else cJSON_AddNullToObject(x, K[i]);
	}
	static const char *MM[5] = { "off", "1x", "2x", "1x rotated", "2x rotated" };
	cJSON_AddStringToObject(x, "maskmode", MM[v.mask_mode >= 0 && v.mask_mode < 5 ? v.mask_mode : 0]);
	if (v.mask_mode && v.mask[0]) cJSON_AddStringToObject(x, "mask", v.mask);
	else cJSON_AddNullToObject(x, "mask");
	if (v.gamma_on && v.gamma[0]) cJSON_AddStringToObject(x, "gamma", v.gamma);
	else cJSON_AddNullToObject(x, "gamma");
	return x;
}

static char *cmd_video(cJSON *req)
{
	const char *sys = cJSON_GetStringValue(cJSON_GetObjectItem(req, "system"));
	char core[64], err[200] = "";
	if (!sys) {
		cJSON *o = ok(), *arr = cJSON_AddArrayToObject(o, "systems");
		for (int i = 0; i < NSYS; i++)
			if (video_core_name(SYS[i].id, core, sizeof(core)) == 0) cJSON_AddItemToArray(arr, video_json(SYS[i].id, core));
		return reply(o);
	}
	if (video_core_name(sys, core, sizeof(core))) return fail("No core installed for that system");
	cJSON *set = cJSON_GetObjectItem(req, "set"), *it;
	const char *preset = cJSON_GetStringValue(cJSON_GetObjectItem(req, "preset"));
	if (cJSON_IsObject(set)) {
		cJSON_ArrayForEach(it, set) {
			const char *v = cJSON_IsString(it) ? it->valuestring : NULL;
			if (video_set(core, it->string, v, err, sizeof(err))) return fail(err);
		}
	} else if (preset) {
		if (video_preset(core, preset, err, sizeof(err))) return fail(err);
	} else if (cJSON_IsTrue(cJSON_GetObjectItem(req, "undo"))) {
		if (video_undo(core, err, sizeof(err))) return fail(err);
	}
	cJSON *o = ok();
	cJSON_AddItemToObject(o, "video", video_json(sys, core));
	return reply(o);
}

static void add_path(void *u, const char *rel) { cJSON_AddItemToArray((cJSON *)u, cJSON_CreateString(rel)); }

static int str_cmp(const void *a, const void *b) { return natcmp(*(const char *const *)a, *(const char *const *)b); }

static char *cmd_video_files(cJSON *req)
{
	const char *kind = cJSON_GetStringValue(cJSON_GetObjectItem(req, "kind"));
	cJSON *tmp = cJSON_CreateArray();
	if (!kind || video_list(kind, add_path, tmp)) { cJSON_Delete(tmp); return fail("kind is filters, masks, gamma or presets"); }
	int n = cJSON_GetArraySize(tmp);
	const char **v = malloc(sizeof(*v) * (size_t)(n ? n : 1));
	for (int i = 0; i < n; i++) v[i] = cJSON_GetArrayItem(tmp, i)->valuestring;
	qsort(v, (size_t)n, sizeof(*v), str_cmp);
	cJSON *o = ok(), *arr = cJSON_AddArrayToObject(o, "files");
	for (int i = 0; i < n; i++) cJSON_AddItemToArray(arr, cJSON_CreateString(v[i]));
	free(v);
	cJSON_Delete(tmp);
	return reply(o);
}

/* diag --kv lines: "area.key=level|message", "summary=level|text" */
static char *cmd_diag(cJSON *req)
{
	if (cJSON_IsTrue(cJSON_GetObjectItem(req, "run"))) diag_run();
	if (cJSON_IsTrue(cJSON_GetObjectItem(req, "bundle"))) diag_bundle();
	diag_state_t s;
	diag_state(&s);
	cJSON *o = ok();
	cJSON_AddBoolToObject(o, "installed", s.installed);
	cJSON_AddStringToObject(o, "busy", s.busy == 1 ? "checking" : s.busy == 2 ? "bundle" : "");
	if (s.at) cJSON_AddNumberToObject(o, "at", (double)s.at);
	else cJSON_AddNullToObject(o, "at");
	if (s.bundle[0]) cJSON_AddStringToObject(o, "bundle", s.bundle);
	else cJSON_AddNullToObject(o, "bundle");
	if (s.error[0]) cJSON_AddStringToObject(o, "error", s.error);
	cJSON *res = cJSON_AddArrayToObject(o, "results");
	char *save = NULL;
	for (char *line = s.output ? strtok_r(s.output, "\n", &save) : NULL; line; line = strtok_r(NULL, "\n", &save)) {
		char *eq = strchr(line, '='), *bar = eq ? strchr(eq, '|') : NULL;
		if (!eq || !bar) continue;
		*eq = 0;
		*bar = 0;
		if (!strcmp(line, "summary")) {
			cJSON *sm = cJSON_AddObjectToObject(o, "summary");
			cJSON_AddStringToObject(sm, "level", eq + 1);
			cJSON_AddStringToObject(sm, "text", bar + 1);
			continue;
		}
		cJSON *x = cJSON_CreateObject();
		char *dot = strchr(line, '.');
		if (dot) { *dot = 0; cJSON_AddStringToObject(x, "area", line); cJSON_AddStringToObject(x, "key", dot + 1); }
		else { cJSON_AddStringToObject(x, "area", ""); cJSON_AddStringToObject(x, "key", line); }
		cJSON_AddStringToObject(x, "level", eq + 1);
		cJSON_AddStringToObject(x, "message", bar + 1);
		cJSON_AddItemToArray(res, x);
	}
	free(s.output);
	return reply(o);
}

static void db_exec_id(sqlite3 *db, const char *sql, long long id)
{
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return;
	sqlite3_bind_int64(st, 1, id);
	sqlite3_step(st);
	sqlite3_finalize(st);
}

static const char *EDITABLE[] = { "title", "description", "year", "developer", "publisher", "genre", "players", "region", NULL };

static char *cmd_edit(sqlite3 *db, cJSON *req)
{
	long long id = req_id(req);
	cJSON *set = cJSON_GetObjectItem(req, "set"), *it;
	if (!cJSON_IsObject(set)) return fail("set missing");
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, "SELECT 1 FROM games WHERE id=?", -1, &st, NULL) != SQLITE_OK) return fail("database");
	sqlite3_bind_int64(st, 1, id);
	int found = sqlite3_step(st) == SQLITE_ROW;
	sqlite3_finalize(st);
	if (!found) return fail("no such game");
	/* check everything first, so a bad field changes nothing */
	cJSON_ArrayForEach(it, set) {
		int known = 0;
		for (int i = 0; EDITABLE[i]; i++) if (!strcmp(it->string, EDITABLE[i])) known = 1;
		if (!known) return fail("can't change that field");
		if (!cJSON_IsString(it) && !cJSON_IsNull(it) && !cJSON_IsNumber(it)) return fail("values must be text");
		if (!strcmp(it->string, "title") && (!cJSON_IsString(it) || !it->valuestring[strspn(it->valuestring, " \t")]))
			return fail("the title can't be empty");
	}
	db_exec(db, "BEGIN");
	cJSON_ArrayForEach(it, set) {
		char sql[96], num[32];
		snprintf(sql, sizeof(sql), "UPDATE games SET %s=? WHERE id=?", it->string);   /* name checked above */
		if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) { db_exec(db, "ROLLBACK"); return fail("database"); }
		const char *v = NULL;
		if (cJSON_IsString(it)) v = it->valuestring;
		else if (cJSON_IsNumber(it)) { snprintf(num, sizeof(num), "%g", it->valuedouble); v = num; }
		while (v && (*v == ' ' || *v == '\t')) v++;
		if (v && *v) sqlite3_bind_text(st, 1, v, -1, SQLITE_TRANSIENT);
		else sqlite3_bind_null(st, 1);
		sqlite3_bind_int64(st, 2, id);
		sqlite3_step(st);
		sqlite3_finalize(st);
	}
	db_exec_id(db, "UPDATE games SET scrape='done' WHERE id=? AND scrape='miss' AND description IS NOT NULL", id);
	db_exec(db, "COMMIT");
	return cmd_game(db, id);
}

static char *cmd_set_art(sqlite3 *db, cJSON *req)
{
	long long id = req_id(req);
	const char *file = cJSON_GetStringValue(cJSON_GetObjectItem(req, "file"));
	if (!file || !*file) return fail("file missing");
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, "SELECT system_id, key FROM games WHERE id=?", -1, &st, NULL) != SQLITE_OK) return fail("database");
	sqlite3_bind_int64(st, 1, id);
	if (sqlite3_step(st) != SQLITE_ROW) { sqlite3_finalize(st); return fail("no such game"); }
	char sys[64], key[512], rel[600];
	snprintf(sys, sizeof(sys), "%s", (const char *)sqlite3_column_text(st, 0));
	snprintf(key, sizeof(key), "%s", (const char *)sqlite3_column_text(st, 1));
	sqlite3_finalize(st);
	if (art_store_box(sys, key, file, rel, sizeof(rel))) return fail("that isn't an image Phosphor can read");
	if (sqlite3_prepare_v2(db, "UPDATE games SET art_box=? WHERE id=?", -1, &st, NULL) != SQLITE_OK) return fail("database");
	sqlite3_bind_text(st, 1, rel, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(st, 2, id);
	sqlite3_step(st);
	sqlite3_finalize(st);
	db_exec_id(db, "UPDATE games SET scrape='done' WHERE id=? AND scrape='miss'", id);   /* not "not found" any more */
	plog("new box art for game %lld", id);
	return cmd_game(db, id);
}

/* values passed to a module script as arguments: never something it could read as an option */
static int arg_ok(const char *v)
{
	return !v || (*v && *v != '-' && !strpbrk(v, "\n\r\"\\") && strlen(v) < 256);
}

/* cloud saves (cloud-sync module): status, storage places, and one background job at a time */
static char *cmd_cloud(cJSON *req)
{
	const char *d = cJSON_GetStringValue(cJSON_GetObjectItem(req, "do"));
	if (d && *d) {
		const char *args[24] = { 0 };
		char store[20][300];
		int a = 0, k = 0;
		const char *from = cJSON_GetStringValue(cJSON_GetObjectItem(req, "from"));
		const char *name = cJSON_GetStringValue(cJSON_GetObjectItem(req, "name"));
		const char *type_ = cJSON_GetStringValue(cJSON_GetObjectItem(req, "type"));
		const char *folder_ = cJSON_GetStringValue(cJSON_GetObjectItem(req, "folder"));
		if ((from && !arg_ok(from)) || (name && !arg_ok(name)) || (type_ && !arg_ok(type_)) || (folder_ && !arg_ok(folder_)))
			return fail("that name isn't allowed");
		if (!strcmp(d, "pull") || !strcmp(d, "preview")) {
			if (from && *from) { args[a++] = "--from"; args[a++] = from; }
			if (cJSON_IsTrue(cJSON_GetObjectItem(req, "force"))) args[a++] = "--force";
		} else if (!strcmp(d, "push")) {
			if (cJSON_IsTrue(cJSON_GetObjectItem(req, "force"))) args[a++] = "--force";
		} else if (!strcmp(d, "use")) {
			const char *folder = cJSON_GetStringValue(cJSON_GetObjectItem(req, "folder"));
			if (!name || !*name) return fail("name missing");
			args[a++] = name;
			if (folder && *folder) args[a++] = folder;
		} else if (!strcmp(d, "add")) {
			const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(req, "type"));
			if (!name || !*name || !type || !*type) return fail("name and type needed");
			/* real storage only (never local/alias/union...), and no settings that run a program or read a file */
			static const char *TYPES[] = { "webdav", "sftp", "smb", "ftp", "drive", "dropbox", "onedrive", "box", "pcloud", "s3", "b2",
			                               "swift", "mega", "koofr", "seafile", "jottacloud", "hidrive", "yandex", "zoho", "putio",
			                               "premiumizeme", "sharefile", "googlephotos", NULL };
			int tok = 0;
			for (int i = 0; TYPES[i]; i++) if (!strcmp(type, TYPES[i])) tok = 1;
			if (!tok) return fail("that storage type isn't supported");
			args[a++] = name;
			args[a++] = type;
			cJSON *it, *set = cJSON_GetObjectItem(req, "settings");
			cJSON_ArrayForEach(it, set) {
				if (!cJSON_IsString(it) || k >= 20 || a >= 22) continue;
				if (!it->string[0] || strspn(it->string, "abcdefghijklmnopqrstuvwxyz_0123456789") != strlen(it->string)) return fail("bad setting name");
				size_t kl = strlen(it->string);
				if (!strcmp(it->string, "ssh") || !strcmp(it->string, "config") || !strcmp(it->string, "env_auth") || strstr(it->string, "command") ||
				    (kl > 5 && !strcmp(it->string + kl - 5, "_file")) || (kl > 5 && !strcmp(it->string + kl - 5, "_exec")))
					return fail("that setting isn't allowed");
				if (strpbrk(it->valuestring, "\n\r")) return fail("settings are one line each");
				snprintf(store[k], sizeof(store[k]), "%s=%s", it->string, it->valuestring);
				args[a++] = store[k++];
			}
		} else if (strcmp(d, "setup") && strcmp(d, "test") && strcmp(d, "devices") && strcmp(d, "history")) {
			return fail("do: setup, add, use, test, push, preview, pull, devices or history");
		}
		int r = cloud_start(d, args);
		if (r == 1) return fail("cloud-sync is busy with something else; try again in a moment");
		if (r < 0) return fail("The cloud-sync module isn't installed");
	}
	cloud_state_t cs;
	cloud_state(&cs);
	cJSON *o = ok();
	cJSON_AddBoolToObject(o, "installed", cs.installed);
	cJSON *st = cJSON_AddObjectToObject(o, "status");
	char *save = NULL;
	for (char *line = cs.status ? strtok_r(cs.status, "\n", &save) : NULL; line; line = strtok_r(NULL, "\n", &save)) {
		char *eq = strchr(line, '=');
		if (!eq) continue;
		*eq = 0;
		cJSON_AddStringToObject(st, line, eq + 1);
	}
	cJSON *rs = cJSON_AddArrayToObject(o, "remotes");
	save = NULL;
	for (char *line = cs.remotes ? strtok_r(cs.remotes, "\n", &save) : NULL; line; line = strtok_r(NULL, "\n", &save)) {
		char *tab = strchr(line, '\t');
		if (!tab) continue;
		*tab = 0;
		cJSON *x = cJSON_CreateObject();
		cJSON_AddStringToObject(x, "name", line);
		char *use = strstr(tab + 1, "  (in use)");
		if (use) *use = 0;
		cJSON_AddStringToObject(x, "type", tab + 1);
		cJSON_AddBoolToObject(x, "in_use", use != NULL);
		cJSON_AddItemToArray(rs, x);
	}
	cJSON *j = cJSON_AddObjectToObject(o, "job");
	cJSON_AddStringToObject(j, "name", cs.job);
	cJSON_AddBoolToObject(j, "busy", cs.busy);
	cJSON_AddBoolToObject(j, "ok", cs.ok);
	cJSON_AddStringToObject(j, "output", cs.output ? cs.output : "");
	cJSON_AddNumberToObject(j, "at", (double)cs.at);
	free(cs.status);
	free(cs.remotes);
	free(cs.output);
	return reply(o);
}

/* snapshots (backup-agent module): list, back up now, preview and restore (everything or only saves) */
static cJSON *ports_json(const ports_state_t *s)
{
	cJSON *p = cJSON_CreateObject();
	cJSON_AddBoolToObject(p, "main_api", s->main_api);
	cJSON_AddBoolToObject(p, "available", s->available);
	cJSON_AddBoolToObject(p, "can_load", s->can_load);
	cJSON_AddBoolToObject(p, "in_game", s->in_game);
	cJSON_AddBoolToObject(p, "on", s->on);
	return p;
}

static char *cmd_ports(cJSON *req)
{
	char err[200];
	/* loading the menu core takes seconds: done by the card job thread, not here */
	if (cJSON_IsTrue(cJSON_GetObjectItem(req, "prepare"))) {
		int r = memcard_job("prepare", 1, NULL, err, sizeof(err));
		if (r < 0) return fail(err);
	}
	ports_state_t s;
	memcard_state(&s);
	cJSON *o = ok();
	cJSON_AddItemToObject(o, "ports", ports_json(&s));
	return reply(o);
}

static char *cmd_pads(void)
{
	char err[200];
	char *r = ports_pads(err, sizeof(err));
	if (!r) return fail(err);
	cJSON *j = cJSON_Parse(r);
	free(r);
	if (!j || !cJSON_IsTrue(cJSON_GetObjectItem(j, "ok"))) { cJSON_Delete(j); return fail("Main didn't answer"); }
	return reply(j);
}

static char *cmd_bluetooth(cJSON *req)
{
	char err[200];
	const char *d = cJSON_GetStringValue(cJSON_GetObjectItem(req, "do"));
	const char *mac = cJSON_GetStringValue(cJSON_GetObjectItem(req, "mac"));
	if (d && *d) {
		if (!strcmp(d, "pair")) {
			int r = bt_pair_start();
			if (r < 0) return fail("No Bluetooth adapter");
		}
		else if (!strcmp(d, "cancel")) bt_pair_cancel();
		else if (!strcmp(d, "forget")) { if (bt_forget(mac, err, sizeof(err))) return fail(err); }
		else if (!strcmp(d, "disconnect")) { if (bt_disconnect(mac, err, sizeof(err))) return fail(err); }
		else return fail("do: pair, cancel, forget or disconnect");
	}
	bt_state_t s;
	bt_state(&s);
	bt_dev_t dev[BT_MAX];
	int n = bt_devices(dev, BT_MAX);
	cJSON *o = ok();
	cJSON_AddBoolToObject(o, "present", s.present);
	cJSON *arr = cJSON_AddArrayToObject(o, "devices");
	for (int i = 0; i < n; i++) {
		cJSON *x = cJSON_CreateObject();
		cJSON_AddStringToObject(x, "mac", dev[i].mac);
		cJSON_AddStringToObject(x, "name", dev[i].name);
		cJSON_AddBoolToObject(x, "connected", dev[i].connected);
		cJSON_AddBoolToObject(x, "input", dev[i].input);
		cJSON_AddStringToObject(x, "icon", dev[i].icon);
		cJSON_AddItemToArray(arr, x);
	}
	cJSON *p = cJSON_AddObjectToObject(o, "pairing");
	cJSON_AddBoolToObject(p, "busy", s.busy);
	cJSON_AddBoolToObject(p, "ok", s.ok);
	cJSON_AddNumberToObject(p, "at", (double)s.at);
	cJSON_AddStringToObject(p, "found", s.found);
	cJSON_AddStringToObject(p, "message", s.message);
	return reply(o);
}

static char *cmd_memcard(cJSON *req)
{
	char err[200];
	const char *d = cJSON_GetStringValue(cJSON_GetObjectItem(req, "do"));
	if (d && *d) {
		cJSON *pt = cJSON_GetObjectItem(req, "port");
		int r = memcard_job(d, cJSON_IsNumber(pt) ? pt->valueint : 0, cJSON_GetStringValue(cJSON_GetObjectItem(req, "name")), err, sizeof(err));
		if (r == 1) return fail("A memory card job is already running; try again when it's done");
		if (r < 0) return fail(err);
	}
	ports_state_t s;
	memcard_state(&s);
	cJSON *o = ok();
	cJSON_AddItemToObject(o, "ports", ports_json(&s));
	cJSON *j = cJSON_AddObjectToObject(o, "job");
	cJSON_AddStringToObject(j, "name", s.job);
	cJSON_AddNumberToObject(j, "port", s.port);
	cJSON_AddBoolToObject(j, "busy", s.busy);
	cJSON_AddBoolToObject(j, "ok", s.ok);
	cJSON_AddNumberToObject(j, "done", s.done);
	cJSON_AddNumberToObject(j, "total", s.total);
	cJSON_AddStringToObject(j, "message", s.message);
	cJSON_AddStringToObject(j, "file", s.file);
	cJSON *arr = cJSON_AddArrayToObject(o, "backups");
	char *list = memcard_list(), *save = NULL;
	for (char *line = list ? strtok_r(list, "\n", &save) : NULL; line; line = strtok_r(NULL, "\n", &save)) {
		char name[128];
		long bytes, at;
		if (sscanf(line, "%127[^\t]\t%ld\t%ld", name, &bytes, &at) != 3) continue;
		cJSON *x = cJSON_CreateObject();
		cJSON_AddStringToObject(x, "name", name);
		cJSON_AddNumberToObject(x, "bytes", (double)bytes);
		cJSON_AddNumberToObject(x, "at", (double)at);
		cJSON_AddItemToArray(arr, x);
	}
	free(list);
	return reply(o);
}

static char *cmd_backup(cJSON *req)
{
	const char *d = cJSON_GetStringValue(cJSON_GetObjectItem(req, "do"));
	if (d && *d) {
		const char *only = cJSON_GetStringValue(cJSON_GetObjectItem(req, "only"));
		const char *bname = cJSON_GetStringValue(cJSON_GetObjectItem(req, "name"));
		if ((bname && !arg_ok(bname)) || (only && !arg_ok(only))) return fail("that name isn't allowed");
		int r = backup_start(d, cJSON_GetStringValue(cJSON_GetObjectItem(req, "name")), only);
		if (r == -2) return fail("do: now, preview or restore (with name)");
		if (r == 1) return fail("A backup or restore is already running; try again in a moment");
		if (r < 0) return fail("The backup-agent module isn't installed");
	}
	backup_state_t bs;
	backup_state(&bs);
	cJSON *o = ok();
	cJSON_AddBoolToObject(o, "installed", bs.installed);
	cJSON *st = cJSON_AddObjectToObject(o, "status");
	char *save = NULL;
	for (char *line = bs.status ? strtok_r(bs.status, "\n", &save) : NULL; line; line = strtok_r(NULL, "\n", &save)) {
		char *eq = strchr(line, '=');
		if (!eq) continue;
		*eq = 0;
		cJSON_AddStringToObject(st, line, eq + 1);
	}
	cJSON *arr = cJSON_AddArrayToObject(o, "snapshots");
	save = NULL;
	for (char *line = bs.list ? strtok_r(bs.list, "\n", &save) : NULL; line; line = strtok_r(NULL, "\n", &save)) {
		char *tab = strchr(line, '\t');
		if (!tab || strncmp(line, "snap-", 5)) continue;
		*tab = 0;
		cJSON *x = cJSON_CreateObject();
		cJSON_AddStringToObject(x, "name", line);
		cJSON_AddNumberToObject(x, "kb", atoi(tab + 1));
		/* snap-YYYYMMDD-HHMMSS[-N]-reason.tar.gz */
		int y, mo, dd, h, mi, se;
		if (sscanf(line, "snap-%4d%2d%2d-%2d%2d%2d", &y, &mo, &dd, &h, &mi, &se) == 6) {
			char when[32];
			snprintf(when, sizeof(when), "%04d-%02d-%02d %02d:%02d", y, mo, dd, h, mi);
			cJSON_AddStringToObject(x, "when", when);
		}
		const char *rs = line + 5 + 15;                         /* after "snap-YYYYMMDD-HHMMSS" */
		if (*rs == '-') rs++;
		while (*rs >= '0' && *rs <= '9') rs++;                  /* a "-2" for two in the same second */
		if (*rs == '-') rs++;
		char reason[64];
		snprintf(reason, sizeof(reason), "%s", rs);
		char *dot = strstr(reason, ".tar.gz");
		if (dot) *dot = 0;
		cJSON_AddStringToObject(x, "reason", reason);
		cJSON_AddItemToArray(arr, x);
	}
	cJSON *j = cJSON_AddObjectToObject(o, "job");
	cJSON_AddStringToObject(j, "name", bs.job);
	cJSON_AddBoolToObject(j, "busy", bs.busy);
	cJSON_AddBoolToObject(j, "ok", bs.ok);
	cJSON_AddStringToObject(j, "output", bs.output ? bs.output : "");
	cJSON_AddNumberToObject(j, "at", (double)bs.at);
	free(bs.status);
	free(bs.list);
	free(bs.output);
	return reply(o);
}

static char *cmd_remote(cJSON *req)
{
	cJSON *on = cJSON_GetObjectItem(req, "on");
	const char *d = cJSON_GetStringValue(cJSON_GetObjectItem(req, "do"));
	if (cJSON_IsBool(on) && remote_set_on(cJSON_IsTrue(on))) return fail("can't save remote.ini");
	if (d && !strcmp(d, "pair")) { if (remote_pair_start()) return fail("no random numbers"); }
	else if (d && !strcmp(d, "stop")) remote_pair_stop();
	else if (d && !strcmp(d, "forget")) {
		const char *id = cJSON_GetStringValue(cJSON_GetObjectItem(req, "id"));
		if (!id || remote_forget(id)) return fail("no such device");
	} else if (d) return fail("do: pair, stop or forget");
	remote_info_t ri;
	remote_info(&ri);
	cJSON *o = ok();
	cJSON_AddBoolToObject(o, "on", ri.on);
	cJSON_AddNumberToObject(o, "port", ri.port);
	cJSON_AddBoolToObject(o, "listening", ri.listening);
	cJSON_AddStringToObject(o, "error", ri.error);
	cJSON *a = cJSON_AddArrayToObject(o, "addresses");
	for (int i = 0; i < ri.naddr; i++) cJSON_AddItemToArray(a, cJSON_CreateString(ri.addr[i]));
	if (ri.code[0]) {
		cJSON *p = cJSON_AddObjectToObject(o, "pairing");
		cJSON_AddStringToObject(p, "code", ri.code);
		cJSON_AddNumberToObject(p, "seconds", ri.code_left);
	} else cJSON_AddNullToObject(o, "pairing");
	cJSON_AddStringToObject(o, "paired", ri.paired);
	cJSON *ds = cJSON_AddArrayToObject(o, "devices");
	for (int i = 0; i < ri.ndev; i++) {
		cJSON *x = cJSON_CreateObject();
		cJSON_AddStringToObject(x, "id", ri.dev[i].id);
		cJSON_AddStringToObject(x, "name", ri.dev[i].name);
		cJSON_AddNumberToObject(x, "created", (double)ri.dev[i].created);
		cJSON_AddNumberToObject(x, "last_seen", (double)ri.dev[i].last_seen);
		cJSON_AddItemToArray(ds, x);
	}
	return reply(o);
}

char *api_handle(sqlite3 *db, const char *request)
{
	cJSON *req = cJSON_Parse(request);
	if (!req) return fail("not JSON");
	cJSON *c = cJSON_GetObjectItem(req, "cmd");
	char *out = NULL;
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
	else if (!strcmp(c->valuestring, "diag")) out = cmd_diag(req);
	else if (!strcmp(c->valuestring, "wifi")) out = cmd_wifi(req);
	else if (!strcmp(c->valuestring, "video")) out = cmd_video(req);
	else if (!strcmp(c->valuestring, "video_files")) out = cmd_video_files(req);
	else if (!strcmp(c->valuestring, "edit")) out = cmd_edit(db, req);
	else if (!strcmp(c->valuestring, "set_art")) out = cmd_set_art(db, req);
	else if (!strcmp(c->valuestring, "remote")) out = cmd_remote(req);
	else if (!strcmp(c->valuestring, "cloud")) out = cmd_cloud(req);
	else if (!strcmp(c->valuestring, "backup")) out = cmd_backup(req);
	else if (!strcmp(c->valuestring, "ports")) out = cmd_ports(req);
	else if (!strcmp(c->valuestring, "pads")) out = cmd_pads();
	else if (!strcmp(c->valuestring, "pad_analog")) {
		cJSON *pt = cJSON_GetObjectItem(req, "port");
		char err[200];
		out = ports_pad_analog(cJSON_IsNumber(pt) ? pt->valueint : 0, err, sizeof(err)) ? fail(err) : reply(ok());
	}
	else if (!strcmp(c->valuestring, "memcard")) out = cmd_memcard(req);
	else if (!strcmp(c->valuestring, "bluetooth")) out = cmd_bluetooth(req);
	else if (!strcmp(c->valuestring, "cores")) {
		const char *d = cJSON_GetStringValue(cJSON_GetObjectItem(req, "do"));
		if (d && !strcmp(d, "update")) {
			int r = cores_update();
			if (r < 0) out = fail("MiSTer's updater (Scripts/update.sh) isn't on the SD card");
		}
		if (!out) {
			cores_state_t s;
			cores_state(&s);
			cJSON *o = ok();
			cJSON_AddBoolToObject(o, "available", s.available);
			cJSON_AddBoolToObject(o, "busy", s.busy);
			cJSON_AddBoolToObject(o, "worked", s.ok);
			cJSON_AddNumberToObject(o, "at", (double)s.at);
			cJSON_AddStringToObject(o, "line", s.line);
			out = reply(o);
		}
	}
	else if (!strcmp(c->valuestring, "samba")) {
		cJSON *on = cJSON_GetObjectItem(req, "on");
		char err[160] = "";
		if (cJSON_IsBool(on) && samba_set(cJSON_IsTrue(on), err, sizeof(err)) != 0) out = fail(err);
		if (!out) {
			samba_state_t s;
			samba_state(&s);
			cJSON *o = ok();
			cJSON_AddBoolToObject(o, "available", s.available);
			cJSON_AddBoolToObject(o, "on", s.on);
			cJSON_AddBoolToObject(o, "running", s.running);
			out = reply(o);
		}
	}
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
