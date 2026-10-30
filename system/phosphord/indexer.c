/* phosphord: finds games on the SD card, USB drives and the network share.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * A scan walks each system's games folders on every root that's present. A file whose size and
 * modified time haven't changed is left alone (only marked as seen). Files on a root that isn't
 * mounted right now are marked offline rather than forgotten, so a USB drive or the share coming
 * back doesn't lose anything. A game whose files are all gone is removed unless it has history
 * (played, favourite, art or details), in which case it's kept, hidden, for when it comes back. */
#include "busy.h"
#include "indexer.h"
#include "db.h"
#include "ident.h"
#include "log.h"
#include "paths.h"
#include "systems.h"
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static int requested, ident_wanted, busy, identifying, generation;   /* nothing until asked */
static volatile int stop_req, listed;               /* "stop" from the scan screen; files listed so far */
static volatile const gsys_t *listing_sys;          /* the system being listed */

void indexer_request(void)
{
	pthread_mutex_lock(&lk);
	requested = 1;
	pthread_cond_signal(&cv);
	pthread_mutex_unlock(&lk);
}

void indexer_identify(void)
{
	pthread_mutex_lock(&lk);
	requested = 1;
	ident_wanted = 1;
	pthread_cond_signal(&cv);
	pthread_mutex_unlock(&lk);
}

void indexer_stop(void)
{
	pthread_mutex_lock(&lk);
	stop_req = 1;
	ident_wanted = 0;
	pthread_mutex_unlock(&lk);
}

void indexer_progress(indexer_progress_t *p)
{
	memset(p, 0, sizeof(*p));
	p->stage = busy ? 1 : identifying ? 2 : 0;
	p->listed = listed;
	const gsys_t *sy = (const gsys_t *)listing_sys;
	if (p->stage == 1 && sy) snprintf(p->system, sizeof(p->system), "%s", sy->name);
	if (p->stage == 2) ident_progress(&p->done, &p->total, p->system, sizeof(p->system), p->file, sizeof(p->file));
	p->pending = ident_wanted || requested;
}

int indexer_busy(void) { return busy; }
int indexer_identifying(void) { return identifying; }
int indexer_generation(void) { return generation; }

typedef struct {
	sqlite3 *db;
	sqlite3_stmt *find_file, *touch_file, *update_file, *add_game, *find_game, *add_file;
	const gsys_t *sys;
	const char *root;
	int bad;            /* the current root: a folder in it couldn't be read (a failing drive, say) */
	int gen, added, changed, seen;
} scan_t;

static void prep(sqlite3 *db, sqlite3_stmt **st, const char *sql)
{
	if (sqlite3_prepare_v2(db, sql, -1, st, NULL) != SQLITE_OK) plog("prepare failed: %s", sqlite3_errmsg(db));
}

static void file_found(scan_t *s, const char *path, const char *name, const struct stat *stt)
{
	s->seen++;
	listed = s->seen;
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
	if (!d) { if (errno != ENOENT) s->bad = 1; return; }
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

/* Every drive Phosphor keeps games on carries its own ID: phosphor/drive.id (id=, role=: "sd" for the
 * SD card, "games" for a USB or NVMe drive with a games folder), written the first time it's seen. The
 * library remembers where each ID was mounted (meta drive_at.<id>). A drive's number moves when another
 * USB drive is plugged in first (a repair stick took usb0, and the NVMe drive came up as usb2): its ID says
 * which drive it is, and its games are moved to its new /media/usb<n> in the library (play history,
 * favourites and art stay with the games). A drive without an ID yet (older Phosphor) is recognised by its
 * games instead: none where the library has them, all of the ones looked at under another root. */
static int mounted(const char *m)
{
	FILE *f = fopen("/proc/mounts", "r");
	if (!f) return 0;
	char l[512], a[256], b[256];
	int r = 0;
	while (!r && fgets(l, sizeof(l), f)) if (sscanf(l, "%255s %255s", a, b) == 2 && !strcmp(b, m)) r = 1;
	fclose(f);
	return r;
}
static void drive_id(const char *m, char *out, size_t n)
{
	char p[300], l[128];
	out[0] = 0;
	snprintf(p, sizeof(p), "%s/phosphor/drive.id", m);
	FILE *f = fopen(p, "r");
	if (f) {
		while (fgets(l, sizeof(l), f)) if (!strncmp(l, "id=", 3)) { snprintf(out, n, "%s", l + 3); out[strcspn(out, "\r\n")] = 0; }
		fclose(f);
		if (out[0]) return;
	}
	int sd = !strcmp(m, p_fat());
	char g[300];
	snprintf(g, sizeof(g), "%s/games", m);
	if (!sd && !dir_exists(g)) return;                     /* (only drives with games get one) */
	unsigned char r[8];
	int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (fd < 0 || read(fd, r, sizeof(r)) != (ssize_t)sizeof(r)) { if (fd >= 0) close(fd); return; }
	close(fd);
	snprintf(out, n, "%02x%02x%02x%02x%02x%02x%02x%02x", r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
	snprintf(g, sizeof(g), "%s/phosphor", m);
	mkdir(g, 0755);
	char tmp[310];
	snprintf(tmp, sizeof(tmp), "%s.new", p);
	if (!(f = fopen(tmp, "w"))) { out[0] = 0; return; }
	time_t t = time(NULL);
	fprintf(f, "# This drive, for Phosphor: which drive it is, whatever /media/usb<n> it comes up as\nid=%s\nrole=%s\nmade=%ld\n", out, sd ? "sd" : "games", (long)t);
	if (fclose(f) || rename(tmp, p)) { unlink(tmp); out[0] = 0; return; }
	plog("library: %s gets the drive ID %s", m, out);
}

int indexer_relocate(sqlite3 *db)
{
	int from[8], to[8], nm = 0, known[8] = { 0 };
	char ids[8][40], k[64], was[64];
	{ char sd[40]; drive_id(p_fat(), sd, sizeof(sd)); }
	/* by ID: where was each drive that's here now */
	for (int n = 0; n < 8; n++) {
		char m[32];
		snprintf(m, sizeof(m), "/media/usb%d", n);
		ids[n][0] = 0;
		if (!mounted(m) || p_repair_drive(n)) continue;
		drive_id(m, ids[n], sizeof(ids[n]));
		if (!ids[n][0]) continue;
		snprintf(k, sizeof(k), "drive_at.%s", ids[n]);
		int o;
		if (db_meta_get(db, k, was, sizeof(was)) && sscanf(was, "/media/usb%d", &o) == 1 && o != n && o >= 0 && o < 8) {
			from[nm] = o; to[nm] = n; nm++;
			known[o] = 1;
		}
		known[n] = 1;
	}
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, "SELECT path FROM files WHERE root=? LIMIT 6", -1, &st, NULL) != SQLITE_OK) return 0;
	/* without an ID yet: by its games */
	for (int n = 0; n < 8; n++) {
		if (known[n]) continue;
		char root[40], paths[6][1024];
		int np = 0, here = 0;
		snprintf(root, sizeof(root), "/media/usb%d/games", n);
		sqlite3_reset(st);
		sqlite3_bind_text(st, 1, root, -1, SQLITE_TRANSIENT);
		while (np < 6 && sqlite3_step(st) == SQLITE_ROW) snprintf(paths[np++], sizeof(paths[0]), "%s", sqlite3_column_text(st, 0));
		if (!np) continue;
		for (int i = 0; i < np; i++) here += access(paths[i], F_OK) == 0;
		if (here) continue;                                 /* (that drive's still there) */
		for (int m = 0; m < 8; m++) {
			if (m == n || p_repair_drive(m)) continue;
			int found = 0;
			for (int i = 0; i < np; i++) {
				char q[1100];
				snprintf(q, sizeof(q), "/media/usb%d%s", m, paths[i] + strlen("/media/usb0"));
				found += access(q, F_OK) == 0;
			}
			if (found == np) { from[nm] = n; to[nm] = m; nm++; break; }
		}
	}
	sqlite3_finalize(st);
	for (int n = 0; n < 8; n++) {
		if (!ids[n][0]) continue;
		char m[32];
		snprintf(k, sizeof(k), "drive_at.%s", ids[n]);
		snprintf(m, sizeof(m), "/media/usb%d", n);
		if (!db_meta_get(db, k, was, sizeof(was)) || strcmp(was, m)) db_meta_set(db, k, m);
	}
	if (!nm) return 0;
	/* (in two steps, through a name no drive has, so two drives that swapped numbers both move) */
	db_exec(db, "BEGIN");
	for (int step = 0; step < 2; step++)
		for (int i = 0; i < nm; i++) {
			char a[40], b[40];
			if (step == 0) { snprintf(a, sizeof(a), "/media/usb%d/", from[i]); snprintf(b, sizeof(b), "/media/usb-moving%d/", to[i]); }
			else { snprintf(a, sizeof(a), "/media/usb-moving%d/", to[i]); snprintf(b, sizeof(b), "/media/usb%d/", to[i]); }
			char where[48];
			snprintf(where, sizeof(where), "%sgames", a);
			char *sql = sqlite3_mprintf("UPDATE OR IGNORE files SET path=%Q || substr(path, %d), root=%Q || substr(root, %d), online=1 "
			                            "WHERE root=%Q", b, (int)strlen(a) + 1, b, (int)strlen(a) + 1, where);
			db_exec(db, sql);
			sqlite3_free(sql);
		}
	db_exec(db, "COMMIT");
	for (int i = 0; i < nm; i++) plog("library: the games on /media/usb%d are on /media/usb%d now: moved there", from[i], to[i]);
	return nm;
}

static void scan(sqlite3 *db)
{
	time_t t0 = time(NULL);
	char buf[32];
	int gen = 1;
	db_meta_get(db, "scan_gen", buf, sizeof(buf));
	if (buf[0]) gen = atoi(buf) + 1;

	indexer_relocate(db);                   /* (drives that came up under another number, before anything's gone) */
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

	listed = 0;
	int bad[16] = { 0 };                    /* roots with a folder that couldn't be read */
	for (int i = 0; i < NSYS && !stop_req; i++) {
		s.sys = &SYS[i];
		listing_sys = &SYS[i];
		db_exec(db, "BEGIN");
		for (int r = 0; r < nr; r++) {
			int arcade = !strcmp(SYS[i].id, "Arcade");
			if (arcade != (r == nr - 1)) continue;
			if (!dir_exists(roots[r])) continue;
			s.root = roots[r];
			s.bad = 0;
			if (arcade) { walk(&s, roots[r], 0); bad[r] |= s.bad; continue; }
			char folders[96], *save = NULL;
			snprintf(folders, sizeof(folders), "%s", SYS[i].folders);
			for (char *f = strtok_r(folders, "|", &save); f; f = strtok_r(NULL, "|", &save)) {
				char dir[600];
				snprintf(dir, sizeof(dir), "%s/%s", roots[r], f);
				if (dir_exists(dir)) walk(&s, dir, 0);
			}
			bad[r] |= s.bad;
		}
		db_exec(db, "COMMIT");
	}

	/* a scan stopped partway didn't reach every system: nothing is taken as gone (the next full scan
	   does it), and the generation stays, so this one's marks still count */
	int stopped = stop_req;
	if (stopped) plog("scan %d stopped: nothing removed", gen);

	/* roots that are present: files not seen are gone (not on a root that had a folder it couldn't
	   read: those files may well be there). Roots that aren't: offline */
	db_exec(db, "BEGIN");
	sqlite3_stmt *gone, *away;
	prep(db, &gone, "DELETE FROM files WHERE root=? AND seen<?");
	prep(db, &away, "UPDATE files SET online=0 WHERE root=?");
	for (int r = 0; r < nr; r++) {
		if (dir_exists(roots[r])) {
			if (stopped || bad[r]) { if (bad[r]) plog("scan: %s had folders that couldn't be read: nothing removed there", roots[r]); continue; }
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
	if (!stopped) db_exec(db, "DELETE FROM games WHERE id NOT IN (SELECT game_id FROM files) AND play_count=0 AND favorite=0 "
	            "AND art_box IS NULL AND description IS NULL AND based_on IS NULL "
	            "AND id NOT IN (SELECT based_on FROM games WHERE based_on IS NOT NULL)");
	snprintf(buf, sizeof(buf), "%d", gen);
	if (!stopped) db_meta_set(db, "scan_gen", buf);
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

static int ident_should_stop(void) { return requested || stop_req || !menu_up(); }

/* Looks for games only when asked ("Look for games"), like Console Mode: never at start-up, when a
 * drive comes or goes, or on a timer. One request lists the files and then works out what they are.
 * If a game is started in between, identifying stops and carries on once the menu is back. */
static void *thread(void *arg)
{
	(void)arg;
	sqlite3 *db = NULL;
	for (;;) {
		pthread_mutex_lock(&lk);
		while (!requested && !ident_wanted) pthread_cond_wait(&cv, &lk);
		if (!requested) {                    /* only identifying left (a game was running): look again in 30 s */
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += 30;
			pthread_cond_timedwait(&cv, &lk, &ts);
		}
		int scan_now = requested;
		requested = 0;
		if (scan_now) stop_req = 0;
		busy = scan_now;
		pthread_mutex_unlock(&lk);
		if (!db && !(db = db_open())) { busy = 0; ident_wanted = 0; continue; }
		/* (not for the 30 s look while a game runs: no card writes then) */
		int marked = scan_now || (ident_wanted && menu_up());
		if (marked) busy_mark("scan", "Look for games");
		if (scan_now) {
			scan(db);
			busy = 0;
			__sync_add_and_fetch(&generation, 1);
		}
		if (ident_wanted && menu_up()) {
			identifying = 1;
			if (ident_pending(db, ident_should_stop) > 0) __sync_add_and_fetch(&generation, 1);
			identifying = 0;
			/* one pass over every file is the job: files that couldn't be read wait for the next
			   Look for games. Only a pass cut short (a game started) carries on by itself. */
			if (!ident_should_stop()) ident_wanted = 0;
		}
		if (marked) busy_clear("scan");
	}
	return NULL;
}

void indexer_start(void)
{
	pthread_t t;
	if (pthread_create(&t, NULL, thread, NULL) == 0) pthread_detach(t);
}
