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
 *   import                                  one-time import of gamelist.xml / media art */
#include "api.h"
#include "art.h"
#include "cJSON.h"
#include "db.h"
#include "importer.h"
#include "log.h"
#include "indexer.h"
#include "play.h"
#include "systems.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PHOSD_VERSION "0.1.0"

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

static cJSON *game_json(sqlite3 *db, long long id)
{
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db,
	        "SELECT id, system_id, title, year, developer, publisher, genre, players, description, art_box, "
	        "favorite, play_count, play_seconds, last_played, region FROM games WHERE id=?", -1, &st, NULL) != SQLITE_OK)
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
	sqlite3_finalize(st);

	cJSON *files = cJSON_AddArrayToObject(g, "files");
	if (sqlite3_prepare_v2(db, "SELECT path, root, size, online FROM files WHERE game_id=? ORDER BY online DESC, id", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(st, 1, id);
		while (sqlite3_step(st) == SQLITE_ROW) {
			cJSON *f = cJSON_CreateObject();
			add_text(f, "path", st, 0);
			cJSON_AddStringToObject(f, "where", where((const char *)sqlite3_column_text(st, 1)));
			cJSON_AddNumberToObject(f, "size", (double)sqlite3_column_int64(st, 2));
			cJSON_AddBoolToObject(f, "online", sqlite3_column_int(st, 3));
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
	if (sqlite3_prepare_v2(db, "SELECT g.id, g.title, g.favorite FROM games g WHERE g.system_id=? AND " VISIBLE
	                           " ORDER BY g.title COLLATE NATSORT LIMIT ? OFFSET ?", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_text(st, 1, sys->valuestring, -1, SQLITE_STATIC);
		sqlite3_bind_int(st, 2, limit);
		sqlite3_bind_int(st, 3, offset);
		while (sqlite3_step(st) == SQLITE_ROW) {
			cJSON *x = cJSON_CreateObject();
			cJSON_AddNumberToObject(x, "id", (double)sqlite3_column_int64(st, 0));
			add_text(x, "title", st, 1);
			cJSON_AddBoolToObject(x, "favorite", sqlite3_column_int(st, 2));
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
