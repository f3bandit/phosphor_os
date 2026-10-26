/* phosphord: starting games and keeping play history.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * A game is started the way MiSTer's own tools do it: an MGL file handed to Main
 * ("load_core <file>" on /dev/MiSTer_cmd); arcade .mra files are loaded directly. When MiSTer's
 * running core goes back to MENU, the time since the launch is added to the game's play time. */
#include "play.h"
#include "ra.h"
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

int play_launch(sqlite3 *db, long long id, char *err, size_t errn)
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
	char sys_id[32], path[1024];
	snprintf(sys_id, sizeof(sys_id), "%s", sqlite3_column_text(st, 0));
	snprintf(path, sizeof(path), "%s", sqlite3_column_text(st, 1));
	sqlite3_finalize(st);
	if (!file_exists(path)) {
		snprintf(err, errn, "%s isn't there any more", path);
		return -1;
	}
	const gsys_t *s = system_by_id(sys_id);
	if (!s) { snprintf(err, errn, "Unknown system %s", sys_id); return -1; }

	char cmd[1300];
	if (!strcmp(s->id, "Arcade")) {
		snprintf(cmd, sizeof(cmd), "load_core %s\n", path);
	} else {
		char rbf[96];
		if (!system_core(s, rbf, sizeof(rbf))) {
			snprintf(err, errn, "The %s core isn't installed", s->name);
			return -1;
		}
		char esc[1100], mgl[600], setname[64] = "";
		xml_escape(esc, sizeof(esc), path);
		/* RetroAchievements: logged in and its core for this system is here -> that core (odelot's), under
		   its RA_ set name (the same games folder: same_dir) */
		char rarbf[96];
		if (ra_core_for(s->id, rarbf, sizeof(rarbf), setname, sizeof(setname))) snprintf(rbf, sizeof(rbf), "%s", rarbf);
		snprintf(mgl, sizeof(mgl), "%s/launch.mgl", p_tmp());
		FILE *f = fopen(mgl, "w");
		if (!f) { snprintf(err, errn, "couldn't write %s", mgl); return -1; }
		fprintf(f, "<mistergamedescription>\n\t<rbf>%s</rbf>\n", rbf);
		if (setname[0]) fprintf(f, "\t<setname same_dir=\"1\">%s</setname>\n", setname);
		fprintf(f, "\t<file delay=\"%d\" type=\"%c\" index=\"%d\" path=\"%s\"/>\n</mistergamedescription>\n",
		        s->delay, s->type, s->index, esc);
		fclose(f);
		snprintf(cmd, sizeof(cmd), "load_core %s\n", mgl);
	}
	if (mister_cmd(cmd, err, errn) != 0) return -1;

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
