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
 *   rescan [stop:true]                      look for games (list them, then identify them); only ever on request.
 *                                           stop: stop it (what's done stays)
 *   scrape_progress                         {running, status, system, game, source, total, done, found, not_found,
 *                                             later, systems:[{name, games, art, pending}]}: the Getting box art screen
 *   scan_progress                           {stage listing|identifying|waiting|idle, listed, done, total, system, file,
 *                                            systems:[{name, files, identified}], games}: for the scan screen
 *   gen                                     {generation, indexing, identifying}: cheap "has the library changed?"
 *   guard [do restore|keep|rollback|delete, items]  updates & restore points (update-guard)
 *   import                                  one-time import of gamelist.xml / media art
 *   link       id base                      id is a hack/mod/translation of base: uses base's art and details
 *   unlink     id                           not based on another game any more
 *   candidates id [q] [limit]               games id could be based on, most alike first: [{id,title,score}]
 *   scrape     [on]                         box art and details: turn on/off; replies on, running,
 *                                           pending, found, not_found, status
 *   dashboard  [kernel]                     live numbers: temps, cpu, load, uptime, mem, disks, io, ips, net,
 *                                           procs, core; kernel: the kernel log's last lines (read only)
 *   storage    [do: check (test, target) | mount (target /dev/sdXN) | overlap-remove | overlap-undo]
 *                                           drives {path,kind,fs,size_kb,free_kb,model,gen,watts,mounted},
 *                                           overlap_tool [paths], last {job,target,busy,ok,at,output}
 *   controllers [live: eventN] [do: style (vidpid, style|auto) | reset (vidpid) | backup [name] |
 *              restore (name) | map (vidpid) | map_skip | map_cancel]   USB / Bluetooth pads: controllers
 *              [{id, name, vidpid, where, buttons, axes, db_name, db_user, style, style_auto, style_set,
 *              has [controls], mapping {uses custom|database|none, menu, cores}}], files, backups, map
 *              (the shell's mapping window: on, step, dev, done, saved); live: {on [controls], ls, rs, l2, r2}
 *   update     [do: install (file) | rollback (version)]   Phosphor's own updates: available, version,
 *              packages [{file, version}], rollbacks [{version, date}], state {state, version, restart, msg, at}
 *   system     do: shutdown|reboot|reload_ui|safe_stop|halt_now|reboot_now   (see the command below)
 *   scrape_accounts [set:{sec,key,value}]  account keys in scraper.ini: {accounts:[{sec,key,set,value}]}
 *   rescrape   id                           look this game up again
 *   wifi       [do: scan|connect|forget|up|down|undo] [ssid] [password] [hidden]
 *                                           {status{...}, scanning, networks:[{ssid, signal, bands,
 *                                           secure, saved}], saved:[{ssid, open}]}; passwords are
 *                                           never sent back
 *   sysbk [get id offset | put id offset size hash data]   MiSTer & Linux backups for the apps (sysbk.c)
 *   look                                    the TV's accent (amber|red|green|blue|purple) and texture number
 *   vset [try {key:value}|keep|revert|undo] [secs]  MiSTer.ini video settings, tried with Keep (vtry.h)
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
 *   samba      [on: true|false]            MiSTer's file sharing (Samba): turn on / off (in the background); available, on,
 *                                           running, busy, want, worked, error
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
 *   memcard    [do: backup|restore|check|prepare|name] [port] [name]   (name: a card waiting for one, see ask_name)
 *                                           memory cards: {ports{...}, job{name, port, busy, ok, done,
 *                                           total, message, file}, backups:[{name, bytes, at}]};
 *                                           restore backs the card up first
 *   remote     [on] [do: pair|stop|forget] [id]
 *                                           remote access (local socket only): {on, port, listening,
 *                                           error, addresses, pairing{code, seconds}|null, paired,
 *                                           devices:[{id, name, created, last_seen}]} */
#include "api.h"
#include "disc.h"
#include "ra.h"
#include "music.h"
#include "art.h"
#include "cJSON.h"
#include "db.h"
#include "diag.h"
#include "wifi.h"
#include "vtry.h"
#include "sysbk.h"
#include "remote.h"
#include "mccmd.h"
#include "cloud.h"
#include "backup.h"
#include "memcard.h"
#include "bt.h"
#include "cores.h"
#include "samba.h"
#include "storage.h"
#include "extras.h"
#include "bgcache.h"
#include "ctrl.h"
#include "dash.h"
#include "version.h"
#include "ident.h"
#include "importer.h"
#include "log.h"
#include "indexer.h"
#include "guard.h"
#include "power.h"
#include "paths.h"
#include "play.h"
#include "scraper.h"
#include "systems.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>


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
	if (sc && !strcmp(sc, "nogame")) return "not_a_game";                         /* a test ROM, a tool: no art expected */
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
		else if (!strcmp(filter, "not_game")) fw = " AND g.scrape='nogame'";
		else if (!strcmp(filter, "unidentified")) fw = " AND g.key LIKE 'n:%'";
		else if (!strcmp(filter, "favorites")) fw = " AND g.favorite=1";
		else if (!strcmp(filter, "recent")) fw = " AND g.last_played IS NOT NULL";
		else return fail("filter: missing_art, missing_details, not_found, linked, not_game, unidentified, favorites or recent");
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
	ra_badge(db, id, g);                     /* RetroAchievements: "ra":{total,unlocked} when known */
	cJSON *o = ok();
	cJSON_AddItemToObject(o, "game", g);
	return reply(o);
}

static char *cmd_last(sqlite3 *db)
{
	char buf[32];
	db_meta_get(db, "last_game", buf, sizeof(buf));
	cJSON *o = ok();
	/* Play last: a disc played after the last game from storage, if it's still in the drive (disc.c) */
	long long played = 0;
	if (buf[0]) {
		sqlite3_stmt *st;
		if (sqlite3_prepare_v2(db, "SELECT IFNULL(last_played,0) FROM games WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
			sqlite3_bind_int64(st, 1, atoll(buf));
			if (sqlite3_step(st) == SQLITE_ROW) played = sqlite3_column_int64(st, 0);
			sqlite3_finalize(st);
		}
	}
	disc_last(db, o, played);
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
	/* "port": a game ported from another system (an NES game on the SNES) is based on the original */
	if (strcmp(s1, s2) && !cJSON_IsTrue(cJSON_GetObjectItem(req, "port"))) return fail("the base game has to be for the same system");
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
	char mysub[512];
	title_subtitle(me, mysub, sizeof(mysub));
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
		if (!q[0] && mysub[0]) {
			/* the same subtitle under another main title: a translated name (Demon Child Zenki /
			   Kishin Douji Zenki - Battle Raiden, #142) */
			char sub[512];
			title_subtitle(t, sub, sizeof(sub));
			if (!strcmp(sub, mysub) && c[n].score < 900) c[n].score = 900;
		}
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

/* the TV's own look (the shell's frontend.ini): accent and texture, so the apps can match it */
static char *cmd_look(void)
{
	static const char *NAMES[] = { "amber", "red", "green", "blue", "purple" };
	char path[600], line[200];
	snprintf(path, sizeof(path), "%s/phosphor/config/frontend.ini", p_fat());
	int accent = 0, texture = 0;
	FILE *f = fopen(path, "r");
	while (f && fgets(line, sizeof(line), f)) {
		if (!strncmp(line, "accent=", 7)) accent = atoi(line + 7);
		else if (!strncmp(line, "texture=", 8)) texture = atoi(line + 8);
	}
	if (f) fclose(f);
	if (accent < 0 || accent > 4) accent = 0;
	cJSON *o = ok();
	cJSON_AddStringToObject(o, "accent", NAMES[accent]);
	cJSON_AddNumberToObject(o, "texture", texture);
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

/* updates & restore points (update-guard): what a Linux update reset, the restore points, and one
 * job at a time (restore some settings, keep stock, roll back, delete points) */
/* {"cmd":"power"} the power check: {count, warn_at, warn, events:[{at, what}]}; "do":"ack" dismisses */
static char *cmd_power(cJSON *req)
{
	const char *d = cJSON_GetStringValue(cJSON_GetObjectItem(req, "do"));
	if (d && strcmp(d, "ack")) return fail("do: ack");
	char *st = power_state(d != NULL);
	cJSON *o = ok();
	cJSON_AddBoolToObject(o, "installed", st != NULL);
	int count = 0, warn_at = 3, warn = 0;
	cJSON *ev = cJSON_CreateArray();
	char *save = NULL;
	for (char *line = st ? strtok_r(st, "\n", &save) : NULL; line; line = strtok_r(NULL, "\n", &save)) {
		if (!strncmp(line, "count=", 6)) count = atoi(line + 6);
		else if (!strncmp(line, "warn_at=", 8)) warn_at = atoi(line + 8);
		else if (!strncmp(line, "warn=", 5)) warn = atoi(line + 5) == 1;
		else if (!strncmp(line, "event=", 6)) {
			char *bar = strchr(line + 6, '|');
			if (!bar) continue;
			*bar = 0;
			cJSON *x = cJSON_CreateObject();
			cJSON_AddNumberToObject(x, "at", atof(line + 6));
			cJSON_AddStringToObject(x, "what", bar + 1);
			cJSON_AddItemToArray(ev, x);
		}
	}
	free(st);
	cJSON_AddNumberToObject(o, "count", count);
	cJSON_AddNumberToObject(o, "warn_at", warn_at);
	cJSON_AddBoolToObject(o, "warn", warn);
	cJSON_AddItemToObject(o, "events", ev);
	return reply(o);
}

static char *cmd_guard(cJSON *req)
{
	const char *d = cJSON_GetStringValue(cJSON_GetObjectItem(req, "do"));
	if (d && *d) {
		const char *args[16];
		int n = 0;
		cJSON *list = cJSON_GetObjectItem(req, "items"), *it;
		cJSON_ArrayForEach(it, list) {
			const char *v = cJSON_GetStringValue(it);
			if (!v || !arg_ok(v) || n >= 16) return fail("that item isn't allowed");
			/* settings files are /etc/... or /root/...; points are plain names */
			if (!strcmp(d, "restore") ? (strncmp(v, "/etc/", 5) && strncmp(v, "/root/", 6)) || strstr(v, "..")
			                          : strchr(v, '/') != NULL) return fail("that item isn't allowed");
			args[n++] = v;
		}
		int r = guard_start(d, args, n);
		if (r == -2) return fail("do: restore (items: files), keep, rollback (items: one point) or delete (items: points)");
		if (r == 1) return fail("Busy with the last one; try again in a moment");
		if (r < 0) return fail("The update-guard module isn't installed");
	}
	guard_state_t gs;
	guard_state(&gs);
	cJSON *o = ok();
	cJSON_AddBoolToObject(o, "installed", gs.installed);
	cJSON *files = cJSON_AddArrayToObject(o, "files"), *points = cJSON_AddArrayToObject(o, "points");
	char *save = NULL;
	for (char *line = gs.ui ? strtok_r(gs.ui, "\n", &save) : NULL; line; line = strtok_r(NULL, "\n", &save)) {
		char *eq = strchr(line, '=');
		if (!eq) continue;
		*eq = 0;
		char *v = eq + 1;
		if (!strcmp(line, "file")) {
			char *bar = strchr(v, '|');
			if (!bar) continue;
			*bar = 0;
			cJSON *f = cJSON_CreateObject();
			cJSON_AddStringToObject(f, "path", v);
			cJSON_AddBoolToObject(f, "reset", bar[1] == '1');
			cJSON_AddItemToArray(files, f);
		} else if (!strcmp(line, "point")) {
			char *fld[5] = { v, "", "", "", "" };
			for (int k = 1; k < 5; k++) { char *b = strchr(fld[k - 1], '|'); if (!b) break; *b = 0; fld[k] = b + 1; }
			cJSON *pt = cJSON_CreateObject();
			cJSON_AddStringToObject(pt, "name", fld[0]);
			cJSON_AddStringToObject(pt, "label", fld[1]);
			cJSON_AddStringToObject(pt, "created", fld[2]);
			cJSON_AddStringToObject(pt, "linux", fld[3]);
			cJSON_AddBoolToObject(pt, "complete", !strcmp(fld[4], "yes"));
			cJSON_AddItemToArray(points, pt);
		} else if (!strcmp(line, "awaiting")) cJSON_AddBoolToObject(o, "awaiting", *v == '1');
		else cJSON_AddStringToObject(o, line, v);       /* linux, updated_from, updated_at */
	}
	cJSON *j = cJSON_AddObjectToObject(o, "job");
	cJSON_AddStringToObject(j, "name", gs.job);
	cJSON_AddBoolToObject(j, "busy", gs.busy);
	cJSON_AddBoolToObject(j, "ok", gs.ok);
	cJSON_AddStringToObject(j, "output", gs.output ? gs.output : "");
	free(gs.ui);
	free(gs.output);
	return reply(o);
}

/* cloud saves (cloud-sync module): status, storage places, and one background job at a time */
static char *cmd_cloud(cJSON *req)
{
	const char *d = cJSON_GetStringValue(cJSON_GetObjectItem(req, "do"));
	if (d && *d) cloud_fresh();
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
	if (d && (!strcmp(d, "library") || !strcmp(d, "card") || !strcmp(d, "export") || !strcmp(d, "edit"))) {
		const char *e = NULL;
		cJSON *o = mc_command(req, &e);
		if (!o) return fail(e ? e : "memory card change failed");
		cJSON_AddBoolToObject(o, "ok", 1);
		return reply(o);
	}
	if (d && *d) {
		cJSON *pt = cJSON_GetObjectItem(req, "port");
		if (!strcmp(d, "name")) {
			char rel[300];
			if (memcard_name(cJSON_GetStringValue(cJSON_GetObjectItem(req, "name")), rel, sizeof(rel), err, sizeof(err))) return fail(err);
		} else {
		int r = memcard_job(d, cJSON_IsNumber(pt) ? pt->valueint : 0, cJSON_GetStringValue(cJSON_GetObjectItem(req, "name")), err, sizeof(err));
		if (r == 1) return fail("A memory card job is already running; try again when it's done");
		if (r < 0) return fail(err);
		}
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
	cJSON_AddStringToObject(j, "map", s.map);     /* 1024 characters, see memcard.h ("" before the first card job) */
	cJSON_AddStringToObject(j, "file", s.file);
	cJSON_AddBoolToObject(j, "ask_name", s.ask_name);
	cJSON_AddStringToObject(j, "suggest", s.suggest);
	cJSON *arr = cJSON_AddArrayToObject(o, "backups");
	char *list = memcard_list(), *save = NULL;
	for (char *line = list ? strtok_r(list, "\n", &save) : NULL; line; line = strtok_r(NULL, "\n", &save)) {
		char name[256];
		long bytes, at;
		if (sscanf(line, "%255[^\t]\t%ld\t%ld", name, &bytes, &at) != 3) continue;
		cJSON *x = cJSON_CreateObject();
		cJSON_AddStringToObject(x, "name", name);
		cJSON_AddNumberToObject(x, "bytes", (double)bytes);
		cJSON_AddNumberToObject(x, "at", (double)at);
		cJSON_AddItemToArray(arr, x);
	}
	free(list);
	return reply(o);
}

/* storage: the drives and the last check; do: a check (test, target) or overlap-remove / overlap-undo */
static char *cmd_storage(cJSON *req)
{
	const char *d = cJSON_GetStringValue(cJSON_GetObjectItem(req, "do"));
	if (d && *d) {
		const char *test = !strcmp(d, "check") ? cJSON_GetStringValue(cJSON_GetObjectItem(req, "test")) : d;
		int r = test ? storage_start(test, cJSON_GetStringValue(cJSON_GetObjectItem(req, "target"))) : -2;
		if (r == -2) return fail("do: check (test, target), mount (target /dev/sdXN), overlap-remove or overlap-undo");
		if (r == 1) return fail("A check is already running; try again when it's done");
		if (r < 0) return fail("The storage module isn't installed");
	}
	storage_state_t ss;
	storage_state(&ss);
	cJSON *o = ok();
	cJSON_AddBoolToObject(o, "installed", ss.installed);
	cJSON *arr = cJSON_AddArrayToObject(o, "drives");
	char *save = NULL;
	for (char *line = ss.targets ? strtok_r(ss.targets, "\n", &save) : NULL; line; line = strtok_r(NULL, "\n", &save)) {
		char *f[9] = { 0 };
		int n = 0;
		for (char *p = line; n < 9; n++) { f[n] = p; char *bar = strchr(p, '|'); if (!bar) { n++; break; } *bar = 0; p = bar + 1; }
		if (n < 7) continue;
		cJSON *x = cJSON_CreateObject();
		cJSON_AddStringToObject(x, "path", f[0]);
		cJSON_AddStringToObject(x, "kind", f[1]);
		cJSON_AddStringToObject(x, "fs", f[2]);
		cJSON_AddNumberToObject(x, "size_kb", atof(f[3]));
		cJSON_AddNumberToObject(x, "free_kb", atof(f[4]));
		cJSON_AddStringToObject(x, "model", f[5]);
		cJSON_AddStringToObject(x, "gen", f[6]);
		cJSON_AddStringToObject(x, "watts", n > 7 && f[7] ? f[7] : "");
		cJSON_AddBoolToObject(x, "mounted", !(n > 8 && f[8] && !strcmp(f[8], "unmounted")));
		cJSON_AddItemToArray(arr, x);
	}
	cJSON *ov = cJSON_AddArrayToObject(o, "overlap_tool");
	if (ss.overlap && !strncmp(ss.overlap, "found=", 6)) {
		char *v = ss.overlap + 6;
		v[strcspn(v, "\n")] = 0;
		save = NULL;
		for (char *t = strtok_r(v, " ", &save); t; t = strtok_r(NULL, " ", &save)) cJSON_AddItemToArray(ov, cJSON_CreateString(t));
	}
	cJSON *last = cJSON_AddObjectToObject(o, "last");
	cJSON_AddStringToObject(last, "job", ss.job);
	cJSON_AddStringToObject(last, "target", ss.target);
	cJSON_AddBoolToObject(last, "busy", ss.busy);
	cJSON_AddBoolToObject(last, "ok", ss.ok);
	cJSON_AddNumberToObject(last, "at", (double)ss.at);
	cJSON_AddStringToObject(last, "output", ss.output ? ss.output : "");
	free(ss.targets); free(ss.overlap); free(ss.output);
	return reply(o);
}

/* Phosphor's own updates (phosphor/update.sh): packages found, versions to roll back to, the last run */
static char *run_out(const char *cmd)
{
	FILE *f = popen(cmd, "r");
	if (!f) return NULL;
	size_t len = 0, cap = 1024;
	char *b = malloc(cap);
	size_t r;
	while (b && (r = fread(b + len, 1, cap - len - 1, f)) > 0) { len += r; if (len + 1 >= cap) { cap *= 2; b = realloc(b, cap); } }
	pclose(f);
	if (b) b[len] = 0;
	return b;
}

static char *upd_run(void *what)
{
	char cmd[700];
	snprintf(cmd, sizeof(cmd), "'%s/phosphor/update.sh' %s 2>/dev/null", p_fat(), (const char *)what);
	return run_out(cmd);
}
static bgcache_t c_upd_list = BGCACHE(upd_run, "list", 20), c_upd_rb = BGCACHE(upd_run, "rollbacks", 60);

static char *cmd_update(cJSON *req)
{
	char up[600], cmd[1400], line[600];
	snprintf(up, sizeof(up), "%s/phosphor/update.sh", p_fat());
	cJSON *o = ok();
	if (access(up, X_OK)) { cJSON_AddBoolToObject(o, "available", 0); return reply(o); }
	const char *d = cJSON_GetStringValue(cJSON_GetObjectItem(req, "do"));
	/* the packages (each one opened to read its version) take seconds: kept, refreshed in the background;
	   an install or roll back looks again first */
	char *pk = bgcache_get(&c_upd_list, 1), *rb = bgcache_get(&c_upd_rb, 1);
	if (d && *d) {
		const char *arg = cJSON_GetStringValue(cJSON_GetObjectItem(req, !strcmp(d, "install") ? "file" : "version"));
		/* only what update.sh itself listed (the kept list; looked at again only when it isn't there) */
		for (int pass = 0; pass < 2; pass++) {
			int found = 0;
			for (const char *l = !strcmp(d, "install") ? pk : !strcmp(d, "rollback") ? rb : NULL; arg && l && *l; ) {
				size_t n = strcspn(l, "\n");
				const char *bar = memchr(l, '|', n);
				if (bar && (size_t)(bar - l) == strlen(arg) && !strncmp(l, arg, (size_t)(bar - l))) found = 1;
				l += n + (l[n] == '\n');
			}
			if (found) break;
			if (pass) { free(pk); free(rb); cJSON_Delete(o); return fail("do: install (file from packages) or rollback (version from rollbacks)"); }
			free(pk); free(rb);
			pk = bgcache_now(&c_upd_list); rb = bgcache_now(&c_upd_rb);
		}
		/* (a moment before it stops Phosphor, so this reply gets out first); one at a time: update.sh
		   turns down a second one with 3 */
		snprintf(cmd, sizeof(cmd), "PHOS_UPDATE_DELAY=2 '%s' %s '%s' > /dev/null 2>&1", up, d, arg);
		plog("update: %s %s", d, arg);
		int rc = system(cmd);
		if (rc != 0) {
			free(pk); free(rb); cJSON_Delete(o);
			return fail(WIFEXITED(rc) && WEXITSTATUS(rc) == 3 ? "An update is already running: wait for it to finish" : "the updater didn't start");
		}
		bgcache_stale(&c_upd_list); bgcache_stale(&c_upd_rb);
	}
	cJSON_AddBoolToObject(o, "available", 1);
	snprintf(cmd, sizeof(cmd), "%s/phosphor/VERSION", p_fat());
	FILE *vf = fopen(cmd, "r");
	if (vf && fgets(line, sizeof(line), vf)) { line[strcspn(line, " \n")] = 0; cJSON_AddStringToObject(o, "version", line); }
	if (vf) fclose(vf);
	const char *keys[2] = { "packages", "rollbacks" }, *f1[2] = { "file", "version" }, *f2[2] = { "version", "date" };
	char *txt[2] = { pk, rb };
	for (int k = 0; k < 2; k++) {
		cJSON *a = cJSON_AddArrayToObject(o, keys[k]);
		for (char *save = NULL, *l = txt[k] ? strtok_r(txt[k], "\n", &save) : NULL; l; l = strtok_r(NULL, "\n", &save)) {
			char *bar = strchr(l, '|');
			if (!bar) continue;
			*bar = 0;
			cJSON *x = cJSON_CreateObject();
			cJSON_AddStringToObject(x, f1[k], l);
			cJSON_AddStringToObject(x, f2[k], bar + 1);
			cJSON_AddItemToArray(a, x);
		}
	}
	free(pk); free(rb);
	snprintf(cmd, sizeof(cmd), "'%s' state 2>/dev/null", up);
	char *st = run_out(cmd);
	cJSON *so = cJSON_AddObjectToObject(o, "state");
	for (char *save = NULL, *l = st ? strtok_r(st, "\n", &save) : NULL; l; l = strtok_r(NULL, "\n", &save)) {
		char *eq = strchr(l, '=');
		if (!eq) continue;
		*eq = 0;
		if (!strcmp(l, "restart") || !strcmp(l, "at")) cJSON_AddNumberToObject(so, l, atof(eq + 1));
		else cJSON_AddStringToObject(so, l, eq + 1);
	}
	free(st);
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
	} else if (d) return fail("do: pair, stop, forget");
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
	else if (!strcmp(c->valuestring, "look")) out = cmd_look();
	else if (!strcmp(c->valuestring, "sysbk")) {
		const char *e = NULL;
		cJSON *o = sysbk_cmd(req, &e);
		if (!o) out = fail(e ? e : "failed");
		else { cJSON_AddBoolToObject(o, "ok", 1); out = reply(o); }
	}
	else if (!strcmp(c->valuestring, "vset")) {
		const char *e = NULL;
		cJSON *o = vt_cmd(req, &e);
		if (!o) out = fail(e ? e : "failed");
		else { cJSON_AddBoolToObject(o, "ok", 1); out = reply(o); }
	}
	else if (!strcmp(c->valuestring, "edit")) out = cmd_edit(db, req);
	else if (!strcmp(c->valuestring, "set_art")) out = cmd_set_art(db, req);
	else if (!strcmp(c->valuestring, "remote")) out = cmd_remote(req);
	else if (!strcmp(c->valuestring, "cloud")) out = cmd_cloud(req);
	else if (!strcmp(c->valuestring, "power")) out = cmd_power(req);
	else if (!strcmp(c->valuestring, "guard")) out = cmd_guard(req);
	else if (!strcmp(c->valuestring, "backup")) out = cmd_backup(req);
	else if (!strcmp(c->valuestring, "storage")) out = cmd_storage(req);
	else if (!strcmp(c->valuestring, "network") || !strcmp(c->valuestring, "usb") || !strcmp(c->valuestring, "ini") || !strcmp(c->valuestring, "scripts")) {
		const char *e = NULL, *w = c->valuestring;
		cJSON *o = !strcmp(w, "network") ? ex_network() : !strcmp(w, "usb") ? ex_usb() : !strcmp(w, "ini") ? ex_ini(req, &e) : ex_scripts(req, &e);
		if (!o) out = fail(e ? e : "failed");
		else { cJSON_AddBoolToObject(o, "ok", 1); out = reply(o); }
	}
	else if (!strcmp(c->valuestring, "press")) {
		/* Remote control (the Windows app): a key into the shell's key FIFO, marked as the app's (the shell
		   keeps those out of network, pairing and remote access settings) */
		static const char *const KEYS[] = { "up", "down", "left", "right", "a", "b", "x", "y", "start", "select", "l", "r", NULL };
		const char *k = cJSON_GetStringValue(cJSON_GetObjectItem(req, "key"));
		int good = 0;
		for (int i = 0; k && KEYS[i]; i++) if (!strcmp(k, KEYS[i])) good = 1;
		if (!good) out = fail("key: up down left right a b x y start select l r");
		else {
			const char *fp = getenv("PHOS_KEYS");
			int fd = open(fp && *fp ? fp : "/tmp/phosphor-keys", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
			char line[24];
			int n = snprintf(line, sizeof(line), "app:%s\n", k);
			struct stat sb;
			if (fd >= 0 && (fstat(fd, &sb) || !S_ISFIFO(sb.st_mode))) { close(fd); fd = -1; }   /* only the shell's FIFO */
			int w = fd >= 0 ? (int)write(fd, line, (size_t)n) : -1;
			if (fd >= 0) close(fd);
			out = w == n ? reply(ok()) : fail("Phosphor's screen isn't running (a game may be on)");
		}
	}
	else if (!strcmp(c->valuestring, "update")) out = cmd_update(req);
	else if (!strcmp(c->valuestring, "controllers")) {
		const char *e = NULL;
		cJSON *o = ctrl_command(req, &e);
		if (!o) out = fail(e ? e : "controllers failed");
		else { cJSON_AddBoolToObject(o, "ok", 1); out = reply(o); }
	}
	else if (!strcmp(c->valuestring, "dashboard")) {
		cJSON *o = dash_json(cJSON_IsTrue(cJSON_GetObjectItem(req, "kernel")));
		cJSON_AddBoolToObject(o, "ok", 1);
		out = reply(o);
	}
	else if (!strcmp(c->valuestring, "ports")) out = cmd_ports(req);
	else if (!strcmp(c->valuestring, "pads")) out = cmd_pads();
	else if (!strcmp(c->valuestring, "scan_progress")) {   /* for the Looking for games screen, while it runs */
		indexer_progress_t pr;
		indexer_progress(&pr);
		cJSON *o = ok();
		cJSON_AddStringToObject(o, "stage", pr.stage == 1 ? "listing" : pr.stage == 2 ? "identifying" : pr.pending ? "waiting" : "idle");
		cJSON_AddNumberToObject(o, "listed", pr.listed);
		cJSON_AddNumberToObject(o, "done", pr.done);
		cJSON_AddNumberToObject(o, "total", pr.total);
		cJSON_AddStringToObject(o, "system", pr.system);
		cJSON_AddStringToObject(o, "file", pr.file);
		cJSON_AddNumberToObject(o, "generation", indexer_generation());
		cJSON *arr = cJSON_AddArrayToObject(o, "systems");
		sqlite3_stmt *st;
		if (sqlite3_prepare_v2(db, "SELECT g.system_id, COUNT(*), SUM(f.ident) FROM files f JOIN games g ON g.id=f.game_id "
		                           "WHERE f.online=1 GROUP BY g.system_id", -1, &st, NULL) == SQLITE_OK) {
			while (sqlite3_step(st) == SQLITE_ROW) {
				const gsys_t *sy = system_by_id((const char *)sqlite3_column_text(st, 0));
				cJSON *e = cJSON_CreateObject();
				cJSON_AddStringToObject(e, "name", sy ? sy->name : (const char *)sqlite3_column_text(st, 0));
				cJSON_AddNumberToObject(e, "files", sqlite3_column_int(st, 1));
				cJSON_AddNumberToObject(e, "identified", sqlite3_column_int(st, 2));
				cJSON_AddItemToArray(arr, e);
			}
			sqlite3_finalize(st);
		}
		int games = 0;
		if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM games g WHERE " VISIBLE, -1, &st, NULL) == SQLITE_OK) {
			if (sqlite3_step(st) == SQLITE_ROW) games = sqlite3_column_int(st, 0);
			sqlite3_finalize(st);
		}
		cJSON_AddNumberToObject(o, "games", games);
		out = reply(o);
	}
	else if (!strcmp(c->valuestring, "scrape_progress")) {   /* for the Getting box art screen, while it's open */
		scrape_progress_t pr;
		scraper_progress(&pr);
		cJSON *o = ok();
		cJSON_AddBoolToObject(o, "running", pr.running);
		cJSON_AddStringToObject(o, "status", pr.status);
		cJSON_AddStringToObject(o, "system", pr.system);
		cJSON_AddStringToObject(o, "game", pr.game);
		cJSON_AddStringToObject(o, "source", pr.source);
		cJSON_AddNumberToObject(o, "total", pr.total);
		cJSON_AddNumberToObject(o, "done", pr.done);
		cJSON_AddNumberToObject(o, "found", pr.found);
		cJSON_AddNumberToObject(o, "not_found", pr.not_found);
		cJSON_AddNumberToObject(o, "later", pr.later);
		cJSON_AddStringToObject(o, "note", pr.note);
		/* per system, in the order the scraper goes through them: games, with box art, still to look up */
		cJSON *arr = cJSON_AddArrayToObject(o, "systems");
		sqlite3_stmt *st;
		if (sqlite3_prepare_v2(db, "SELECT g.system_id, COUNT(*), SUM(COALESCE(g.art_box,'') <> ''),"
		                           " SUM(g.scrape IS NULL AND g.based_on IS NULL AND g.key NOT LIKE 'n:%')"
		                           " FROM games g WHERE " VISIBLE " GROUP BY g.system_id ORDER BY g.system_id", -1, &st, NULL) == SQLITE_OK) {
			while (sqlite3_step(st) == SQLITE_ROW) {
				const gsys_t *sy = system_by_id((const char *)sqlite3_column_text(st, 0));
				cJSON *e = cJSON_CreateObject();
				cJSON_AddStringToObject(e, "name", sy ? sy->name : (const char *)sqlite3_column_text(st, 0));
				cJSON_AddNumberToObject(e, "games", sqlite3_column_int(st, 1));
				cJSON_AddNumberToObject(e, "art", sqlite3_column_int(st, 2));
				cJSON_AddNumberToObject(e, "pending", sqlite3_column_int(st, 3));
				cJSON_AddItemToArray(arr, e);
			}
			sqlite3_finalize(st);
		}
		out = reply(o);
	}
	else if (!strcmp(c->valuestring, "gen")) {          /* has the library changed? (cheap: no queries) */
		cJSON *o = ok();
		cJSON_AddNumberToObject(o, "generation", indexer_generation());
		cJSON_AddBoolToObject(o, "indexing", indexer_busy());
		cJSON_AddBoolToObject(o, "identifying", indexer_identifying());
		out = reply(o);
	}
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
	else if (!strcmp(c->valuestring, "system")) {
		/* do:
		 *   shutdown | reboot   the TV's shell does it (its power window's safe stop, with the "safe to
		 *                       switch off" screen): asked through <tmp>/power-request; if the shell isn't
		 *                       there to take it within 20 s, it's done here (safe stop, then halt / reboot)
		 *   safe_stop           phosphor.sh safe-stop now: drives flushed and read-only (exFAT clean);
		 *                       replies drives [{path, state clean|DIRTY|n/a, ro}]. Nothing writes after it.
		 *   halt_now | reboot_now   after safe_stop (the shell's power window)
		 *   reload_ui           restarts the shell only (phosphor.sh reload) */
		const char *act = cJSON_GetStringValue(cJSON_GetObjectItem(req, "do"));
		char sh[1200], req_path[600];
		snprintf(req_path, sizeof(req_path), "%s/power-request", p_tmp());
		const char *ps = "/phosphor/phosphor.sh";
		int run = 1;
		if (act && (!strcmp(act, "shutdown") || !strcmp(act, "reboot"))) {
			FILE *f = fopen(req_path, "w");
			if (f) { fprintf(f, "%s\n", act); fclose(f); }
			snprintf(sh, sizeof(sh), "sleep 20; [ -f '%s' ] || exit 0; rm -f '%s'; '%s%s' safe-stop > /dev/null 2>&1; %s",
			         req_path, req_path, p_fat(), ps, !strcmp(act, "shutdown") ? "halt -f" : "reboot");
		} else if (act && !strcmp(act, "safe_stop")) {
			run = 0;
			unlink(req_path);
			char cmd[700];
			snprintf(cmd, sizeof(cmd), "'%s%s' safe-stop 2>&1", p_fat(), ps);
			plog("system: safe stop (drives read-only from here)");
			cJSON *o = ok(), *arr = cJSON_AddArrayToObject(o, "drives");
			FILE *pp = getenv("PHOS_NO_REBOOT") ? NULL : popen(cmd, "r");
			char line[400];
			int done = 0;
			while (pp && fgets(line, sizeof(line), pp)) {
				line[strcspn(line, "\r\n")] = 0;
				if (!strcmp(line, "done")) { done = 1; continue; }
				char *b1 = strchr(line, '|'), *b2 = b1 ? strchr(b1 + 1, '|') : NULL;
				if (!b1 || !b2) continue;
				*b1 = *b2 = 0;
				cJSON *x = cJSON_CreateObject();
				cJSON_AddStringToObject(x, "path", line);
				cJSON_AddStringToObject(x, "state", b1 + 1);
				cJSON_AddBoolToObject(x, "ro", !strcmp(b2 + 1, "ro"));
				cJSON_AddItemToArray(arr, x);
			}
			if (pp) pclose(pp);
			if (getenv("PHOS_NO_REBOOT")) {            /* tests: a clean SD card */
				cJSON *x = cJSON_CreateObject();
				cJSON_AddStringToObject(x, "path", "/media/fat");
				cJSON_AddStringToObject(x, "state", "clean");
				cJSON_AddBoolToObject(x, "ro", 1);
				cJSON_AddItemToArray(arr, x);
				done = 1;
			}
			cJSON_AddBoolToObject(o, "done", done);
			out = reply(o);
		} else if (act && !strcmp(act, "halt_now")) snprintf(sh, sizeof(sh), "sleep 1; halt -f");
		else if (act && !strcmp(act, "reboot_now")) snprintf(sh, sizeof(sh), "sleep 1; reboot");
		else if (act && !strcmp(act, "reload_ui")) snprintf(sh, sizeof(sh), "sleep 1; '%s%s' reload", p_fat(), ps);
		else { cJSON_Delete(req); return fail("do: shutdown, reboot, reload_ui, safe_stop, halt_now or reboot_now"); }
		if (run) {
			plog("system: %s", act);
			if (getenv("PHOS_NO_REBOOT") && strcmp(act, "reload_ui")) out = reply(ok());     /* tests */
			else {
				pid_t pid = fork();
				if (pid == 0) {
					setsid();
					pid_t q = fork();                   /* not our child: phosphord may be stopped by it */
					if (q == 0) { execl("/bin/sh", "sh", "-c", sh, (char *)NULL); _exit(127); }
					_exit(0);
				}
				if (pid > 0) waitpid(pid, NULL, 0);
				out = pid > 0 ? reply(ok()) : fail("couldn't start it");
			}
		}
	}
	else if (!strcmp(c->valuestring, "samba")) {
		cJSON *on = cJSON_GetObjectItem(req, "on");
		if (cJSON_IsBool(on) && samba_start(cJSON_IsTrue(on)) < 0) out = fail("Couldn't start");
		if (!out) {
			samba_state_t s;
			samba_state(&s);
			cJSON *o = ok();
			cJSON_AddBoolToObject(o, "available", s.available);
			cJSON_AddBoolToObject(o, "on", s.on);
			cJSON_AddBoolToObject(o, "running", s.running);
			cJSON_AddBoolToObject(o, "busy", s.busy);
			cJSON_AddBoolToObject(o, "want", s.want);
			cJSON_AddBoolToObject(o, "worked", s.ok);
			cJSON_AddStringToObject(o, "error", s.error);
			out = reply(o);
		}
	}
	else if (!strcmp(c->valuestring, "scrape_todo")) {
		cJSON *o = scraper_todo(db, req);
		cJSON_AddBoolToObject(o, "ok", 1);
		out = reply(o);
	} else if (!strcmp(c->valuestring, "scrape_result")) {
		const char *e = scraper_result(db, req);
		out = e ? fail(e) : reply(ok());
	} else if (!strcmp(c->valuestring, "scrape_undo")) {
		const char *e = scraper_undo(db, req);
		out = e ? fail(e) : reply(ok());
	} else if (!strcmp(c->valuestring, "scrape_pc_done")) {
		scraper_pc_done(cJSON_GetStringValue(cJSON_GetObjectItem(req, "status")));
		out = reply(ok());
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
	} else if (!strcmp(c->valuestring, "scrape_accounts")) {
		/* set: {sec, key, value}; replies what's set (user names shown, secrets only as set) */
		cJSON *set = cJSON_GetObjectItem(req, "set");
		if (cJSON_IsObject(set)) {
			const char *sec = cJSON_GetStringValue(cJSON_GetObjectItem(set, "sec"));
			const char *key = cJSON_GetStringValue(cJSON_GetObjectItem(set, "key"));
			const char *val = cJSON_GetStringValue(cJSON_GetObjectItem(set, "value"));
			if (!sec || !key || !scraper_account_ok(sec, key)) { cJSON_Delete(req); return fail("not an account setting"); }
			if (scraper_set_account(sec, key, val ? val : "")) { cJSON_Delete(req); return fail("can't save scraper.ini"); }
		}
		scrape_account_t a[SCRAPE_ACCOUNTS];
		int n = 0;
		scraper_accounts(a, &n);
		cJSON *o = ok(), *arr = cJSON_AddArrayToObject(o, "accounts");
		for (int i = 0; i < n; i++) {
			cJSON *x = cJSON_CreateObject();
			cJSON_AddStringToObject(x, "sec", a[i].sec);
			cJSON_AddStringToObject(x, "key", a[i].key);
			cJSON_AddBoolToObject(x, "set", a[i].set);
			cJSON_AddStringToObject(x, "value", a[i].value);
			cJSON_AddItemToArray(arr, x);
		}
		out = reply(o);
	} else if (!strcmp(c->valuestring, "rescrape")) {
		out = scraper_retry(db, req_id(req)) == 0 ? reply(ok()) : fail("no such game");
	} else if (!strcmp(c->valuestring, "not_game")) {
		/* {"id", "on"}: a test ROM, a boot file, a tool: out of "not found", no box art looked up; off puts it back */
		long long gid = req_id(req);
		int on = !cJSON_IsFalse(cJSON_GetObjectItem(req, "on"));
		sqlite3_stmt *st;
		int n = 0;
		if (sqlite3_prepare_v2(db, on ? "UPDATE games SET scrape='nogame' WHERE id=? AND based_on IS NULL"
		                              : "UPDATE games SET scrape='miss' WHERE id=? AND scrape='nogame'", -1, &st, NULL) == SQLITE_OK) {
			sqlite3_bind_int64(st, 1, gid);
			sqlite3_step(st);
			n = sqlite3_changes(db);
			sqlite3_finalize(st);
		}
		out = n ? reply(ok()) : fail(on ? "no such game, or it's linked to another" : "it isn't marked as not a game");
	}
	else if (!strcmp(c->valuestring, "disc")) {
		/* the dock's CD/DVD drive (disc.c): what's in it, named, with box art and whether it plays */
		cJSON *d = disc_json(db);
		if (!d) out = fail("this Main doesn't read discs");
		else { cJSON *o = ok(); cJSON_AddItemToObject(o, "disc", d); out = reply(o); }
	} else if (!strcmp(c->valuestring, "disc_play")) {
		char err[300];
		out = disc_play(db, err, sizeof(err)) == 0 ? reply(ok()) : fail(err);
	} else if (!strcmp(c->valuestring, "disc_enable")) {
		disc_set_enabled(db, cJSON_IsTrue(cJSON_GetObjectItem(req, "on")));
		out = reply(ok());
	} else if (!strcmp(c->valuestring, "disc_eject")) {
		char err[300];
		out = disc_eject(err, sizeof(err)) == 0 ? reply(ok()) : fail(err);
	}
	/* the music player (music.c) */
	else if (!strcmp(c->valuestring, "music")) {
		cJSON *o = ok();
		cJSON_AddItemToObject(o, "music", music_status());
		out = reply(o);
	} else if (!strcmp(c->valuestring, "music_albums")) {
		if (cJSON_IsTrue(cJSON_GetObjectItem(req, "rescan"))) music_scan();
		cJSON *o = ok();
		cJSON_AddItemToObject(o, "albums", music_albums());
		cJSON *cdj = music_cd();
		if (cdj) cJSON_AddItemToObject(o, "cd", cdj);
		out = reply(o);
	} else if (!strcmp(c->valuestring, "music_album")) {
		const char *id = cJSON_GetStringValue(cJSON_GetObjectItem(req, "id"));
		cJSON *a = id ? music_album(id) : NULL;
		if (!a) out = fail("no such album");
		else { cJSON *o = ok(); cJSON_AddItemToObject(o, "album", a); out = reply(o); }
	} else if (!strcmp(c->valuestring, "music_play")) {
		char err[300];
		const char *id = cJSON_GetStringValue(cJSON_GetObjectItem(req, "album"));
		int tr = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(req, "track"));
		out = music_play(id ? id : "", tr, err, sizeof(err)) == 0 ? reply(ok()) : fail(err);
	} else if (!strcmp(c->valuestring, "music_ctl")) {
		char err[300];
		const char *d = cJSON_GetStringValue(cJSON_GetObjectItem(req, "do"));
		out = music_control(d ? d : "", req, err, sizeof(err)) == 0 ? reply(ok()) : fail(err);
	} else if (!strcmp(c->valuestring, "music_queue")) {
		cJSON *o = ok();
		cJSON_AddItemToObject(o, "queue", music_queue());
		out = reply(o);
	}
	/* RetroAchievements (ra.c) */
	else if (!strcmp(c->valuestring, "ra")) {
		cJSON *o = ok();
		cJSON_AddItemToObject(o, "ra", ra_status());
		out = reply(o);
	} else if (!strcmp(c->valuestring, "ra_login")) {
		char err[300];
		const char *u = cJSON_GetStringValue(cJSON_GetObjectItem(req, "user"));
		const char *pw = cJSON_GetStringValue(cJSON_GetObjectItem(req, "password"));
		out = ra_login(u, pw, err, sizeof(err)) == 0 ? reply(ok()) : fail(err);
	} else if (!strcmp(c->valuestring, "ra_logout")) {
		ra_logout();
		out = reply(ok());
	} else if (!strcmp(c->valuestring, "ra_set")) {
		char err[300];
		out = ra_set(req, err, sizeof(err)) == 0 ? reply(ok()) : fail(err);
	} else if (!strcmp(c->valuestring, "ra_key")) {
		char err[300];
		const char *k = cJSON_GetStringValue(cJSON_GetObjectItem(req, "key"));
		out = ra_set_key(k ? k : "", err, sizeof(err)) == 0 ? reply(ok()) : fail(err);
	} else if (!strcmp(c->valuestring, "ra_cores")) {
		char err[300];
		out = ra_get_cores(err, sizeof(err)) == 0 ? reply(ok()) : fail(err);
	} else if (!strcmp(c->valuestring, "ra_match")) {
		char err[300];
		out = ra_match(err, sizeof(err)) == 0 ? reply(ok()) : fail(err);
	} else if (!strcmp(c->valuestring, "ra_profile")) {
		char err[300];
		cJSON *p = ra_profile(db, err, sizeof(err));
		if (!p) out = fail(err);
		else { cJSON *o = ok(); cJSON_AddItemToObject(o, "profile", p); out = reply(o); }
	} else if (!strcmp(c->valuestring, "ra_game")) {
		char err[300];
		cJSON *g = ra_game(db, req_id(req), err, sizeof(err));
		if (!g) out = fail(err);
		else { cJSON *o = ok(); cJSON_AddItemToObject(o, "ra", g); out = reply(o); }
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
		if (cJSON_IsTrue(cJSON_GetObjectItem(req, "stop"))) indexer_stop();
		else indexer_identify();     /* the user asked: list the games and work out what they are */
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
