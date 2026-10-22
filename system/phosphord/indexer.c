/* phosphord: finds games on the SD card, USB drives and the network share.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * A scan walks each system's games folders on every root that's present. A file whose size and
 * modified time haven't changed is left alone (only marked as seen). Files on a root that isn't
 * mounted right now are marked offline rather than forgotten, so a USB drive or the share coming
 * back doesn't lose anything. A game whose files are all gone is removed unless it has history
 * (played, favourite, art or details), in which case it's kept, hidden, for when it comes back. */
#include "indexer.h"
#include "db.h"
#include "ident.h"
#include "scraper.h"
#include "log.h"
#include "paths.h"
#include "systems.h"
#include <dirent.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static int requested = 1, busy, identifying, generation;

void indexer_request(void)
{
	pthread_mutex_lock(&lk);
	requested = 1;
	pthread_cond_signal(&cv);
	pthread_mutex_unlock(&lk);
}

int indexer_busy(void) { return busy; }
int indexer_identifying(void) { return identifying; }
int indexer_generation(void) { return generation; }

typedef struct {
	sqlite3 *db;
	sqlite3_stmt *find_file, *touch_file, *update_file, *add_game, *find_game, *add_file;
	const gsys_t *sys;
	const char *root;
	int gen, added, changed, seen;
} scan_t;

static void prep(sqlite3 *db, sqlite3_stmt **st, const char *sql)
{
	if (sqlite3_prepare_v2(db, sql, -1, st, NULL) != SQLITE_OK) plog("prepare failed: %s", sqlite3_errmsg(db));
}

static void file_found(scan_t *s, const char *path, const char *name, const struct stat *stt)
{
	s->seen++;
	sqlite3_reset(s->find_file);
	sqlite3_bind_text(s->find_file, 1, path, -1, SQLITE_STATIC);
	if (sqlite3_step(s->find_file) == SQLITE_ROW) {
		sqlite3_int64 id = sqlite3_column_int64(s->find_file, 0);
		sqlite3_int64 size = sqlite3_column_int64(s->find_file, 1), mtime = sqlite3_column_int64(s->find_file, 2);
		if (size == (sqlite3_int64)stt->st_size && mtime == (sqlite3_int64)stt->st_mtime) {
			sqlite3_reset(s->touch_file);
			sqlite3_bind_int(s->touch_file, 1, s->gen);
			sqlite3_bind_int64(s->touch_file, 2, id);
			sqlite3_step(s->touch_file);
		} else {
			/* changed: new size/date, and any hash is stale */
			sqlite3_reset(s->update_file);
			sqlite3_bind_int64(s->update_file, 1, (sqlite3_int64)stt->st_size);
			sqlite3_bind_int64(s->update_file, 2, (sqlite3_int64)stt->st_mtime);
			sqlite3_bind_int(s->update_file, 3, s->gen);
			sqlite3_bind_int64(s->update_file, 4, id);
			sqlite3_step(s->update_file);
			s->changed++;
		}
		return;
	}
	/* new file: its game is found by name until it's hashed */
	char title[512], key[600];
	snprintf(title, sizeof(title), "%s", name);
	char *dot = strrchr(title, '.');
	if (dot) *dot = 0;
	game_key(title, key, sizeof(key));
	sqlite3_reset(s->add_game);
	sqlite3_bind_text(s->add_game, 1, s->sys->id, -1, SQLITE_STATIC);
	sqlite3_bind_text(s->add_game, 2, key, -1, SQLITE_STATIC);
	sqlite3_bind_text(s->add_game, 3, title, -1, SQLITE_STATIC);
	sqlite3_bind_int64(s->add_game, 4, (sqlite3_int64)time(NULL));
	sqlite3_step(s->add_game);
	sqlite3_reset(s->find_game);
	sqlite3_bind_text(s->find_game, 1, s->sys->id, -1, SQLITE_STATIC);
	sqlite3_bind_text(s->find_game, 2, key, -1, SQLITE_STATIC);
	if (sqlite3_step(s->find_game) != SQLITE_ROW) return;
	sqlite3_int64 gid = sqlite3_column_int64(s->find_game, 0);
	sqlite3_reset(s->add_file);
	sqlite3_bind_int64(s->add_file, 1, gid);
	sqlite3_bind_text(s->add_file, 2, path, -1, SQLITE_STATIC);
	sqlite3_bind_text(s->add_file, 3, s->root, -1, SQLITE_STATIC);
	sqlite3_bind_int64(s->add_file, 4, (sqlite3_int64)stt->st_size);
	sqlite3_bind_int64(s->add_file, 5, (sqlite3_int64)stt->st_mtime);
	sqlite3_bind_int(s->add_file, 6, s->gen);
	sqlite3_step(s->add_file);
	s->added++;
}

static void walk(scan_t *s, const char *dir, int depth)
{
	DIR *d = opendir(dir);
	if (!d) return;
	struct dirent *e;
	char path[1024];
	while ((e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
		struct stat st;
		if (stat(path, &st) != 0) continue;
		if (S_ISDIR(st.st_mode)) {
			if (depth < 3 && strcmp(e->d_name, "media")) walk(s, path, depth + 1);   /* skip art folders */
		} else if (S_ISREG(st.st_mode) && has_ext(e->d_name, s->sys->exts)) {
			file_found(s, path, e->d_name, &st);
		}
	}
	closedir(d);
}

static void scan(sqlite3 *db)
{
	time_t t0 = time(NULL);
	char buf[32];
	int gen = 1;
	db_meta_get(db, "scan_gen", buf, sizeof(buf));
	if (buf[0]) gen = atoi(buf) + 1;

	scan_t s = { .db = db, .gen = gen };
	prep(db, &s.find_file, "SELECT id, size, mtime FROM files WHERE path=?");
	prep(db, &s.touch_file, "UPDATE files SET seen=?, online=1 WHERE id=?");
	prep(db, &s.update_file, "UPDATE files SET size=?, mtime=?, crc32=NULL, serial=NULL, ident=0, seen=?, online=1 WHERE id=?");
	prep(db, &s.add_game, "INSERT OR IGNORE INTO games(system_id, key, title, added) VALUES(?,?,?,?)");
	prep(db, &s.find_game, "SELECT id FROM games WHERE system_id=? AND key=?");
	prep(db, &s.add_file, "INSERT INTO files(game_id, path, root, size, mtime, seen, online) VALUES(?,?,?,?,?,?,1)");

	/* systems table for clients */
	db_exec(db, "BEGIN");
	db_exec(db, "DELETE FROM systems");
	sqlite3_stmt *sys_ins;
	prep(db, &sys_ins, "INSERT INTO systems(id, name, category) VALUES(?,?,?)");
	for (int i = 0; i < NSYS; i++) {
		sqlite3_reset(sys_ins);
		sqlite3_bind_text(sys_ins, 1, SYS[i].id, -1, SQLITE_STATIC);
		sqlite3_bind_text(sys_ins, 2, SYS[i].name, -1, SQLITE_STATIC);
		sqlite3_bind_int(sys_ins, 3, SYS[i].cat);
		sqlite3_step(sys_ins);
	}
	sqlite3_finalize(sys_ins);
	db_exec(db, "COMMIT");

	/* which roots are here this time */
	char roots[16][256];
	int nr = p_roots(roots, 14);
	snprintf(roots[nr++], 256, "%s/_Arcade", p_fat());

	for (int i = 0; i < NSYS; i++) {
		s.sys = &SYS[i];
		db_exec(db, "BEGIN");
		for (int r = 0; r < nr; r++) {
			int arcade = !strcmp(SYS[i].id, "Arcade");
			if (arcade != (r == nr - 1)) continue;
			if (!dir_exists(roots[r])) continue;
			s.root = roots[r];
			if (arcade) { walk(&s, roots[r], 0); continue; }
			char folders[96], *save = NULL;
			snprintf(folders, sizeof(folders), "%s", SYS[i].folders);
			for (char *f = strtok_r(folders, "|", &save); f; f = strtok_r(NULL, "|", &save)) {
				char dir[600];
				snprintf(dir, sizeof(dir), "%s/%s", roots[r], f);
				if (dir_exists(dir)) walk(&s, dir, 0);
			}
		}
		db_exec(db, "COMMIT");
	}

	/* roots that are present: files not seen are gone. Roots that aren't: offline */
	db_exec(db, "BEGIN");
	sqlite3_stmt *gone, *away;
	prep(db, &gone, "DELETE FROM files WHERE root=? AND seen<?");
	prep(db, &away, "UPDATE files SET online=0 WHERE root=?");
	for (int r = 0; r < nr; r++) {
		if (dir_exists(roots[r])) {
			sqlite3_reset(gone);
			sqlite3_bind_text(gone, 1, roots[r], -1, SQLITE_STATIC);
			sqlite3_bind_int(gone, 2, gen);
			sqlite3_step(gone);
		} else {
			sqlite3_reset(away);
			sqlite3_bind_text(away, 1, roots[r], -1, SQLITE_STATIC);
			sqlite3_step(away);
		}
	}
	sqlite3_finalize(gone);
	sqlite3_finalize(away);
	/* games with no files left and nothing worth keeping */
	db_exec(db, "DELETE FROM games WHERE id NOT IN (SELECT game_id FROM files) AND play_count=0 AND favorite=0 "
	            "AND art_box IS NULL AND description IS NULL AND based_on IS NULL "
	            "AND id NOT IN (SELECT based_on FROM games WHERE based_on IS NOT NULL)");
	snprintf(buf, sizeof(buf), "%d", gen);
	db_meta_set(db, "scan_gen", buf);
	db_exec(db, "COMMIT");

	sqlite3_finalize(s.find_file); sqlite3_finalize(s.touch_file); sqlite3_finalize(s.update_file);
	sqlite3_finalize(s.add_game); sqlite3_finalize(s.find_game); sqlite3_finalize(s.add_file);
	plog("scan %d: %d files (%d new, %d changed) in %lds", gen, s.seen, s.added, s.changed, (long)(time(NULL) - t0));
}

/* identifying reads whole files: only while the menu is up, never while a core runs from the drive */
static int menu_up(void)
{
	char core[64] = "";
	FILE *f = fopen(p_corename(), "r");
	if (!f) return 1;
	if (!fgets(core, sizeof(core), f)) core[0] = 0;
	fclose(f);
	core[strcspn(core, "\r\n")] = 0;
	return !core[0] || !strcmp(core, "MENU");
}

static int ident_should_stop(void) { return requested || !menu_up(); }

static void *thread(void *arg)
{
	(void)arg;
	sqlite3 *db = db_open();
	if (!db) return NULL;
	for (;;) {
		pthread_mutex_lock(&lk);
		if (!requested) {
			/* wake up now and then to carry on identifying (e.g. once a game is closed) */
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += 15;
			pthread_cond_timedwait(&cv, &lk, &ts);
		}
		int scan_now = requested;
		requested = 0;
		busy = scan_now;
		pthread_mutex_unlock(&lk);
		if (scan_now) {
			scan(db);
			busy = 0;
			__sync_add_and_fetch(&generation, 1);
		}
		if (menu_up()) {
			identifying = 1;
			if (ident_pending(db, ident_should_stop) > 0) { __sync_add_and_fetch(&generation, 1); scraper_kick(); }
			identifying = 0;
		}
	}
	return NULL;
}

void indexer_start(void)
{
	pthread_t t;
	if (pthread_create(&t, NULL, thread, NULL) == 0) pthread_detach(t);
}
