/* phosphord: starting games and keeping play history.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * A game is started the way MiSTer's own tools do it: an MGL file handed to Main
 * ("load_core <file>" on /dev/MiSTer_cmd); arcade .mra files are loaded directly. When MiSTer's
 * running core goes back to MENU, the time since the launch is added to the game's play time. */
#include "play.h"
#include "ra.h"
#include "music.h"
#include "db.h"
#include "log.h"
#include "paths.h"
#include "systems.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static long long playing_id;
static time_t playing_since;
static int left_menu;

int mister_cmd(const char *cmd, char *err, size_t errn)
{
	const char *dev = p_mister_cmd();
	int fd = open(dev, O_WRONLY | O_NONBLOCK | O_APPEND | (strncmp(dev, "/dev/", 5) ? O_CREAT : 0), 0644);
	if (fd < 0) {
		snprintf(err, errn, "MiSTer isn't listening on %s (%s)", dev, strerror(errno));
		return -1;
	}
	ssize_t w = write(fd, cmd, strlen(cmd));
	close(fd);
	if (w < 0) {
		snprintf(err, errn, "couldn't send the command to MiSTer (%s)", strerror(errno));
		return -1;
	}
	plog("sent: %.*s", (int)strcspn(cmd, "\n"), cmd);
	return 0;
}

static void xml_escape(char *dst, size_t n, const char *src)
{
	size_t j = 0;
	for (; *src && j + 7 < n; src++) {
		const char *rep = NULL;
		switch (*src) {
		case '&': rep = "&amp;"; break;
		case '<': rep = "&lt;"; break;
		case '>': rep = "&gt;"; break;
		case '"': rep = "&quot;"; break;
		case '\'': rep = "&apos;"; break;
		}
		if (rep) { size_t l = strlen(rep); memcpy(dst + j, rep, l); j += l; }
		else dst[j++] = *src;
	}
	dst[j] = 0;
}

static void corename(char *buf, size_t n)
{
	buf[0] = 0;
	FILE *f = fopen(p_corename(), "r");
	if (!f) return;
	if (!fgets(buf, (int)n, f)) buf[0] = 0;
	fclose(f);
	buf[strcspn(buf, "\r\n")] = 0;
}

/* the cores a game can start on: its system's own (and the alternatives systems.ini names), the
   RetroAchievements one when logged in, and other systems' cores that take the same kind of file (a Game
   Boy game on the Game Boy Color core, say). The first is the one a plain launch uses. */
typedef struct { char rbf[96], setname[64], label[160]; const gsys_t *sys; } core_opt_t;
#define MAX_CORE_OPTS 12

static int base_name(const char *rbf, char *out, size_t n)
{
	const char *sl = strrchr(rbf, '/');
	return snprintf(out, n, "%s", sl ? sl + 1 : rbf);
}

static int core_installed(const char *rbf)
{
	gsys_t one;
	memset(&one, 0, sizeof(one));
	snprintf(one.id, sizeof(one.id), "x");
	snprintf(one.rbf, sizeof(one.rbf), "%s", rbf);
	char got[96];
	return system_core(&one, got, sizeof(got));
}

static int add_opt(core_opt_t *o, int n, const char *rbf, const char *setname, const char *label, const gsys_t *sys)
{
	if (n >= MAX_CORE_OPTS) return n;
	for (int i = 0; i < n; i++) if (!strcmp(o[i].rbf, rbf)) return n;
	snprintf(o[n].rbf, sizeof(o[n].rbf), "%s", rbf);
	snprintf(o[n].setname, sizeof(o[n].setname), "%s", setname ? setname : "");
	snprintf(o[n].label, sizeof(o[n].label), "%s", label);
	o[n].sys = sys;
	return n + 1;
}

/* the game's system and first file that's here; -1 (err set) when it can't start */
static int game_file(sqlite3 *db, long long id, const gsys_t **sys, char *path, size_t pn, char *err, size_t errn)
{
	sqlite3_stmt *st;
	/* the game's system, and its first file that's here (roots in MiSTer's order) */
	if (sqlite3_prepare_v2(db,
	        "SELECT g.system_id, f.path FROM games g JOIN files f ON f.game_id=g.id "
	        "WHERE g.id=? AND f.online=1 ORDER BY f.id LIMIT 1", -1, &st, NULL) != SQLITE_OK) {
		snprintf(err, errn, "database: %s", sqlite3_errmsg(db));
		return -1;
	}
	sqlite3_bind_int64(st, 1, id);
	if (sqlite3_step(st) != SQLITE_ROW) {
		sqlite3_finalize(st);
		snprintf(err, errn, "This game's files aren't available right now");
		return -1;
	}
	char sys_id[32];
	snprintf(sys_id, sizeof(sys_id), "%s", sqlite3_column_text(st, 0));
	snprintf(path, pn, "%s", sqlite3_column_text(st, 1));
	sqlite3_finalize(st);
	if (!file_exists(path)) {
		snprintf(err, errn, "%s isn't there any more", path);
		return -1;
	}
	*sys = system_by_id(sys_id);
	if (!*sys) { snprintf(err, errn, "Unknown system %s", sys_id); return -1; }
	return 0;
}

static int core_opts(const gsys_t *s, const char *path, core_opt_t *o)
{
	int n = 0;
	char rbf[96], label[160], b[64];
	/* the RetroAchievements core first when it's there: that's what a plain launch picks */
	char rarbf[96], setname[64];
	if (ra_core_for(s->id, rarbf, sizeof(rarbf), setname, sizeof(setname))) {
		snprintf(label, sizeof(label), "%s (RetroAchievements)", s->name);
		n = add_opt(o, n, rarbf, setname, label, s);
	}
	/* the system's own, and its alternatives */
	char rbfs[96], *save = NULL;
	snprintf(rbfs, sizeof(rbfs), "%s", s->rbf);
	for (char *c = strtok_r(rbfs, "|", &save); c; c = strtok_r(NULL, "|", &save)) {
		if (!core_installed(c)) continue;
		base_name(c, b, sizeof(b));
		if (strchr(s->rbf, '|')) snprintf(label, sizeof(label), "%s (%s core)", s->name, b);
		else snprintf(label, sizeof(label), "%s", s->name);
		n = add_opt(o, n, c, NULL, label, s);
	}
	/* other systems that take this kind of file */
	const char *fn = strrchr(path, '/');
	fn = fn ? fn + 1 : path;
	for (int i = 0; i < NSYS; i++) {
		const gsys_t *t = &SYS[i];
		if (t == s || !strcmp(t->id, "Arcade") || !has_ext(fn, t->exts)) continue;
		if (!system_core(t, rbf, sizeof(rbf))) continue;
		snprintf(label, sizeof(label), "%s core", t->name);
		n = add_opt(o, n, rbf, NULL, label, t);
	}
	return n;
}

cJSON *play_cores(sqlite3 *db, long long id, char *err, size_t errn)
{
	const gsys_t *s;
	char path[1024];
	if (game_file(db, id, &s, path, sizeof(path), err, errn)) return NULL;
	cJSON *a = cJSON_CreateArray();
	if (strcmp(s->id, "Arcade")) {
		core_opt_t o[MAX_CORE_OPTS];
		int n = core_opts(s, path, o);
		for (int i = 0; i < n; i++) {
			cJSON *x = cJSON_CreateObject();
			char b[64];
			base_name(o[i].setname[0] ? o[i].setname : o[i].rbf, b, sizeof(b));
			cJSON_AddNumberToObject(x, "index", i);
			cJSON_AddStringToObject(x, "name", o[i].label);
			cJSON_AddStringToObject(x, "core", b);        /* the core's name as MiSTer shows it (CORENAME) */
			cJSON_AddItemToArray(a, x);
		}
	}
	return a;
}

/* the play time of the game that's running, before another one starts straight from it */
static void add_play_time(sqlite3 *db)
{
	if (!playing_id || !left_menu) return;
	long secs = (long)(time(NULL) - playing_since);
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, "UPDATE games SET play_seconds=play_seconds+? WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(st, 1, secs);
		sqlite3_bind_int64(st, 2, playing_id);
		sqlite3_step(st);
		sqlite3_finalize(st);
	}
	plog("played game %lld for %lds (another started from it)", playing_id, secs);
	playing_id = 0;
}

int play_launch_core(sqlite3 *db, long long id, int core, char *err, size_t errn)
{
	const gsys_t *s;
	char path[1024];
	if (game_file(db, id, &s, path, sizeof(path), err, errn)) return -1;

	char cmd[1300];
	if (!strcmp(s->id, "Arcade")) {
		snprintf(cmd, sizeof(cmd), "load_core %s\n", path);
	} else {
		core_opt_t o[MAX_CORE_OPTS];
		int n = core_opts(s, path, o);
		if (!n) { snprintf(err, errn, "The %s core isn't installed", s->name); return -1; }
		if (core < 0) core = 0;                   /* (the RetroAchievements one when it's there, else the system's) */
		if (core >= n) { snprintf(err, errn, "That core isn't there any more"); return -1; }
		const core_opt_t *c = &o[core];
		char esc[1100], mgl[600];
		xml_escape(esc, sizeof(esc), path);
		snprintf(mgl, sizeof(mgl), "%s/launch.mgl", p_tmp());
		FILE *f = fopen(mgl, "w");
		if (!f) { snprintf(err, errn, "couldn't write %s", mgl); return -1; }
		/* RetroAchievements' core under its RA_ set name (the same games folder: same_dir) */
		fprintf(f, "<mistergamedescription>\n\t<rbf>%s</rbf>\n", c->rbf);
		if (c->setname[0]) fprintf(f, "\t<setname same_dir=\"1\">%s</setname>\n", c->setname);
		fprintf(f, "\t<file delay=\"%d\" type=\"%c\" index=\"%d\" path=\"%s\"/>\n</mistergamedescription>\n",
		        c->sys->delay, c->sys->type, c->sys->index, esc);
		fclose(f);
		snprintf(cmd, sizeof(cmd), "load_core %s\n", mgl);
	}
	music_stop();                            /* music only plays while no game runs */
	add_play_time(db);                       /* (a game started from another: the first one's time) */
	if (mister_cmd(cmd, err, errn) != 0) return -1;

	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, "UPDATE games SET play_count=play_count+1, last_played=? WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(st, 1, (sqlite3_int64)time(NULL));
		sqlite3_bind_int64(st, 2, id);
		sqlite3_step(st);
		sqlite3_finalize(st);
	}
	char v[32];
	snprintf(v, sizeof(v), "%lld", id);
	db_meta_set(db, "last_game", v);
	playing_id = id;
	playing_since = time(NULL);
	left_menu = 0;
	return 0;
}

int play_launch(sqlite3 *db, long long id, char *err, size_t errn) { return play_launch_core(db, id, -1, err, errn); }

void play_track(sqlite3 *db, long long id)
{
	if (id > 0) {
		sqlite3_stmt *st;
		if (sqlite3_prepare_v2(db, "UPDATE games SET play_count=play_count+1, last_played=? WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
			sqlite3_bind_int64(st, 1, (sqlite3_int64)time(NULL));
			sqlite3_bind_int64(st, 2, id);
			sqlite3_step(st);
			sqlite3_finalize(st);
		}
	}
	playing_id = id > 0 ? id : 0;
	playing_since = time(NULL);
	left_menu = 0;
}

void play_tick(sqlite3 *db)
{
	if (!playing_id) return;
	char c[64];
	corename(c, sizeof(c));
	int in_menu = !c[0] || !strcmp(c, "MENU");
	if (!in_menu) { left_menu = 1; return; }
	if (!left_menu) {
		/* the core hasn't started yet; give up waiting after a minute */
		if (time(NULL) - playing_since > 60) playing_id = 0;
		return;
	}
	long secs = (long)(time(NULL) - playing_since);
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, "UPDATE games SET play_seconds=play_seconds+? WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(st, 1, secs);
		sqlite3_bind_int64(st, 2, playing_id);
		sqlite3_step(st);
		sqlite3_finalize(st);
	}
	plog("played game %lld for %lds", playing_id, secs);
	playing_id = 0;
}
