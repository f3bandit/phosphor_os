/* phosphord: the library database.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * library.db lives on the SD card (data/). WAL mode lets the indexer write while the API reads;
 * synchronous=NORMAL keeps writes cheap on an SD card while staying consistent after power loss
 * (the worst case is losing the last moments of indexing, which the next scan redoes). */
#include "db.h"
#include "log.h"
#include "paths.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define SCHEMA_VERSION 3

static const char *SCHEMA =
	"CREATE TABLE IF NOT EXISTS systems ("
	"  id TEXT PRIMARY KEY, name TEXT NOT NULL, category INTEGER NOT NULL);"
	"CREATE TABLE IF NOT EXISTS games ("
	"  id INTEGER PRIMARY KEY,"
	"  system_id TEXT NOT NULL,"
	"  key TEXT NOT NULL,"                 /* identity: n:<name> until hashed, later crc:/serial: */
	"  title TEXT NOT NULL,"
	"  region TEXT, year TEXT, developer TEXT, publisher TEXT, genre TEXT, players TEXT,"
	"  description TEXT,"
	"  art_box TEXT,"                      /* relative to data/art */
	"  favorite INTEGER NOT NULL DEFAULT 0,"
	"  play_count INTEGER NOT NULL DEFAULT 0,"
	"  play_seconds INTEGER NOT NULL DEFAULT 0,"
	"  last_played INTEGER,"
	"  added INTEGER,"
	"  UNIQUE (system_id, key));"
	"CREATE TABLE IF NOT EXISTS files ("
	"  id INTEGER PRIMARY KEY,"
	"  game_id INTEGER NOT NULL REFERENCES games(id) ON DELETE CASCADE,"
	"  path TEXT NOT NULL UNIQUE,"
	"  root TEXT NOT NULL,"                /* the games root (or _Arcade) it was found under */
	"  size INTEGER, mtime INTEGER,"
	"  crc32 TEXT, serial TEXT,"
	"  online INTEGER NOT NULL DEFAULT 1,"
	"  seen INTEGER NOT NULL DEFAULT 0);"
	"CREATE INDEX IF NOT EXISTS files_game ON files(game_id);"
	"CREATE INDEX IF NOT EXISTS files_root ON files(root);"
	"CREATE INDEX IF NOT EXISTS games_system ON games(system_id);"
	"CREATE TABLE IF NOT EXISTS meta (k TEXT PRIMARY KEY, v TEXT);";

/* 2: identity work done per file; a game can be "based on" another (a hack, a mod, a translation)
 * and then borrows its art and details; scrape remembers a lookup that found nothing */
static const char *MIGRATE_2 =
	"ALTER TABLE files ADD COLUMN ident INTEGER NOT NULL DEFAULT 0;"
	"ALTER TABLE games ADD COLUMN based_on INTEGER REFERENCES games(id) ON DELETE SET NULL;"
	"ALTER TABLE games ADD COLUMN scrape TEXT;"
	"CREATE INDEX IF NOT EXISTS games_based_on ON games(based_on);"
	"CREATE INDEX IF NOT EXISTS files_ident ON files(ident, online);";

/* 3: libretro's game lists (official names, details) and which entry a game matched */
static const char *MIGRATE_3 =
	"CREATE TABLE IF NOT EXISTS ref ("
	"  id INTEGER PRIMARY KEY, lrsys TEXT NOT NULL, name TEXT NOT NULL, crc TEXT, serial TEXT, size INTEGER,"
	"  description TEXT, developer TEXT, publisher TEXT, genre TEXT, year TEXT, players TEXT);"
	"CREATE INDEX IF NOT EXISTS ref_crc ON ref(crc);"
	"CREATE INDEX IF NOT EXISTS ref_serial ON ref(serial);"
	"CREATE INDEX IF NOT EXISTS ref_name ON ref(lrsys, name);"
	"ALTER TABLE games ADD COLUMN ref_id INTEGER;"
	"ALTER TABLE games ADD COLUMN ref_name TEXT;"
	"CREATE INDEX IF NOT EXISTS games_scrape ON games(scrape);";

/* natural order: "Quest 2" before "Quest 10"; case doesn't matter */
int natcmp(const char *a, const char *b)
{
	while (*a && *b) {
		if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
			while (*a == '0' && isdigit((unsigned char)a[1])) a++;
			while (*b == '0' && isdigit((unsigned char)b[1])) b++;
			const char *ea = a, *eb = b;
			while (isdigit((unsigned char)*ea)) ea++;
			while (isdigit((unsigned char)*eb)) eb++;
			if (ea - a != eb - b) return (int)((ea - a) - (eb - b));
			int c = strncmp(a, b, (size_t)(ea - a));
			if (c) return c;
			a = ea; b = eb;
			continue;
		}
		int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
		if (ca != cb) return ca - cb;
		a++; b++;
	}
	return (unsigned char)*a - (unsigned char)*b;
}

static int coll_natural(void *u, int la, const void *a, int lb, const void *b)
{
	(void)u;
	char x[512], y[512];
	snprintf(x, sizeof(x), "%.*s", la, (const char *)a);
	snprintf(y, sizeof(y), "%.*s", lb, (const char *)b);
	return natcmp(x, y);
}

int db_exec(sqlite3 *db, const char *sql)
{
	char *err = NULL;
	int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
	if (rc != SQLITE_OK) {
		plog("sql error: %s", err ? err : "?");
		sqlite3_free(err);
	}
	return rc;
}

const char *db_meta_get(sqlite3 *db, const char *k, char *buf, int n)
{
	sqlite3_stmt *st;
	buf[0] = 0;
	if (sqlite3_prepare_v2(db, "SELECT v FROM meta WHERE k=?", -1, &st, NULL) != SQLITE_OK) return buf;
	sqlite3_bind_text(st, 1, k, -1, SQLITE_STATIC);
	if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_text(st, 0))
		snprintf(buf, (size_t)n, "%s", sqlite3_column_text(st, 0));
	sqlite3_finalize(st);
	return buf;
}

void db_meta_set(sqlite3 *db, const char *k, const char *v)
{
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, "INSERT INTO meta(k,v) VALUES(?,?) ON CONFLICT(k) DO UPDATE SET v=excluded.v", -1, &st, NULL) != SQLITE_OK) return;
	sqlite3_bind_text(st, 1, k, -1, SQLITE_STATIC);
	sqlite3_bind_text(st, 2, v, -1, SQLITE_STATIC);
	sqlite3_step(st);
	sqlite3_finalize(st);
}

void game_key(const char *title, char *key, int n)
{
	int j = snprintf(key, (size_t)n, "n:");
	for (const char *p = title; *p && j + 1 < n; p++) key[j++] = (char)tolower((unsigned char)*p);
	key[j] = 0;
}

sqlite3 *db_open(void)
{
	char path[600];
	mkdirs(p_data());
	snprintf(path, sizeof(path), "%s/library.db", p_data());
	sqlite3 *db;
	if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL) != SQLITE_OK) {
		plog("can't open %s: %s", path, sqlite3_errmsg(db));
		sqlite3_close(db);
		return NULL;
	}
	sqlite3_busy_timeout(db, 5000);
	sqlite3_create_collation(db, "NATSORT", SQLITE_UTF8, NULL, coll_natural);
	/* WAL needs shared memory on the file system; fall back to a rollback journal if it can't */
	sqlite3_stmt *st;
	char mode[16] = "";
	if (sqlite3_prepare_v2(db, "PRAGMA journal_mode=WAL", -1, &st, NULL) == SQLITE_OK) {
		if (sqlite3_step(st) == SQLITE_ROW) snprintf(mode, sizeof(mode), "%s", sqlite3_column_text(st, 0));
		sqlite3_finalize(st);
	}
	db_exec(db, "PRAGMA synchronous=NORMAL; PRAGMA foreign_keys=ON;");
	int ver = 0;
	if (sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &st, NULL) == SQLITE_OK) {
		if (sqlite3_step(st) == SQLITE_ROW) ver = sqlite3_column_int(st, 0);
		sqlite3_finalize(st);
	}
	if (ver < SCHEMA_VERSION) {
		if (ver < 1 && db_exec(db, SCHEMA) != SQLITE_OK) { sqlite3_close(db); return NULL; }
		if (ver < 2 && db_exec(db, MIGRATE_2) != SQLITE_OK) { sqlite3_close(db); return NULL; }
		if (ver < 3 && db_exec(db, MIGRATE_3) != SQLITE_OK) { sqlite3_close(db); return NULL; }
		char v[64];
		snprintf(v, sizeof(v), "PRAGMA user_version=%d", SCHEMA_VERSION);
		db_exec(db, v);
		plog("library database ready (%s, schema %d, journal %s)", path, SCHEMA_VERSION, mode);
	}
	return db;
}
