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
#include "vtry.h"
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include <strings.h>
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

/* the cores a game can start on, all of them its own system's: the system's core (and the alternatives
   systems.ini names), the RetroAchievements one when logged in, older versions, a test build, a dual RAM
   build, other builds in _Other and Y/C builds (S-Video / composite). Never another system's core. The first is the one a plain launch uses. */
typedef struct { char rbf[96], setname[64], label[160], kind[12], date[12]; const gsys_t *sys; } core_opt_t;
#define MAX_CORE_OPTS 16

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
	snprintf(o[n].kind, sizeof(o[n].kind), "mister");
	o[n].date[0] = 0;
	o[n].sys = sys;
	return n + 1;
}

/* a core file's date (NES_20260823.rbf -> 2026-08-23); "" without one */
static void rbf_date(const char *fname, char *out, size_t n)
{
	out[0] = 0;
	const char *u = strrchr(fname, '_');
	if (u && strlen(u) >= 9 && strspn(u + 1, "0123456789") == 8)
		snprintf(out, n, "%.4s-%.2s-%.2s", u + 1, u + 5, u + 7);
}

/* every dated file of a core (dir relative to the SD card, base "NES"), newest first: names without .rbf */
static int rbf_versions(const char *dir, const char *base, char out[][96], int max)
{
	char d[600];
	snprintf(d, sizeof(d), "%s/%s", p_fat(), dir);
	DIR *dd = opendir(d);
	if (!dd) return 0;
	int n = 0;
	size_t bl = strlen(base);
	struct dirent *e;
	while ((e = readdir(dd)) && n < max) {
		size_t l = strlen(e->d_name);
		if (l < 5 || strcasecmp(e->d_name + l - 4, ".rbf") || strncasecmp(e->d_name, base, bl) || e->d_name[bl] != '_') continue;
		snprintf(out[n], 96, "%.*s", (int)(l - 4), e->d_name);
		n++;
	}
	closedir(dd);
	for (int i = 1; i < n; i++)                           /* newest (largest date) first */
		for (int j = i; j > 0 && strcmp(out[j], out[j - 1]) > 0; j--) {
			char t[96]; memcpy(t, out[j], 96); memcpy(out[j], out[j - 1], 96); memcpy(out[j - 1], t, 96);
		}
	return n;
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

#define DUAL_RAM_DIR "_Console (Dual SDRAM)"

/* a core file's name without its date (PSX2XCPU_20260413 -> PSX2XCPU); 0 when it has no _YYYYMMDD */
static int rbf_stem(const char *fname, char *out, size_t n)
{
	size_t l = strlen(fname);
	if (l < 10 || fname[l - 9] != '_') return 0;
	for (size_t i = l - 8; i < l; i++) if (fname[i] < '0' || fname[i] > '9') return 0;
	snprintf(out, n, "%.*s", (int)(l - 9), fname);
	return 1;
}

/* another system's core name (GBA for GB): never taken as an alternative build */
static int is_system_base(const char *stem)
{
	char b[96], *save = NULL;
	for (int i = 0; i < NSYS; i++) {
		char r[96];
		snprintf(r, sizeof(r), "%s", SYS[i].rbf);
		for (char *c = strtok_r(r, "|", &save); c; c = strtok_r(NULL, "|", &save)) {
			base_name(c, b, sizeof(b));
			if (!strcasecmp(b, stem)) return 1;
		}
	}
	return 0;
}

/* other builds of a core in one folder: its name, or its name with something added, the newest of each.
   _Other: alternative builds (ajgowans' Alt Cores: Genesis, PSX2XCPU, N64_80MHz).
   _YCConsole / _YCComputer: Y/C builds, S-Video and composite made by the core (MikeS11's Y/C Builds:
   PSXYC2, PSX2XYC, NES_NativeCVBS). */
static void alt_label(const char *v, int yc, const gsys_t *s, char *out, size_t n)
{
	if (!yc) {
		if (*v) snprintf(out, n, "%s (%s core)", s->name, v);
		else snprintf(out, n, "%s (alternative core)", s->name);
		return;
	}
	const char *y = strstr(v, "YC");
	int digits = y && y > v && strspn(v, "0123456789") == (size_t)(y - v);
	if (digits) snprintf(out, n, "%s (%s%.*s, S-Video / composite)", s->name, s->id, (int)(y - v), v);   /* MSX1 */
	else if (y && y > v) snprintf(out, n, "%s (%.*s, S-Video / composite)", s->name, (int)(y - v), v);       /* 2X */
	else if (strstr(v, "CVBS")) snprintf(out, n, "%s (native composite)", s->name);
	else snprintf(out, n, "%s (S-Video / composite)", s->name);
}
static int alt_opts(core_opt_t *o, int n, const char *base, const gsys_t *s, const char *dir, int yc)
{
	char d[600];
	snprintf(d, sizeof(d), "%s/%s", p_fat(), dir);
	DIR *dd = opendir(d);
	if (!dd) return n;
	char best[8][96] = {{0}}, stems[8][96] = {{0}};
	int nb = 0;
	size_t bl = strlen(base);
	struct dirent *e;
	while ((e = readdir(dd))) {
		size_t l = strlen(e->d_name);
		if (l < 5 || strcasecmp(e->d_name + l - 4, ".rbf") || strncasecmp(e->d_name, base, bl)) continue;
		char f[96], stem[96];
		snprintf(f, sizeof(f), "%.*s", (int)(l - 4), e->d_name);
		if (!rbf_stem(f, stem, sizeof(stem))) {
			if (!yc) continue;                          /* (a Y/C build without a date: NES_NativeCVBS) */
			snprintf(stem, sizeof(stem), "%s", f);
		}
		if (strlen(stem) > bl && is_system_base(stem)) continue;
		if (yc && !strstr(stem + bl, "YC") && !strstr(stem + bl, "CVBS")) continue;
		int k = 0;
		while (k < nb && strcasecmp(stems[k], stem)) k++;
		if (k == nb) { if (nb == 8) continue; snprintf(stems[nb], 96, "%s", stem); nb++; }
		if (strcmp(f, best[k]) > 0) snprintf(best[k], 96, "%s", f);
	}
	closedir(dd);
	for (int k = 0; k < nb && n < MAX_CORE_OPTS; k++) {
		const char *v = stems[k] + bl;
		while (*v == '_' || *v == '-') v++;
		char rbf[130], label[160];
		snprintf(rbf, sizeof(rbf), "%s/%s", dir, best[k]);
		alt_label(v, yc, s, label, sizeof(label));
		int j = n;
		n = add_opt(o, n, rbf, NULL, label, s);
		if (n > j) { snprintf(o[j].kind, sizeof(o[0].kind), yc ? "yc" : "alt"); rbf_date(best[k], o[j].date, sizeof(o[0].date)); }
	}
	return n;
}

static int kind_rank(const char *k) { return !strcmp(k, "version") ? 1 : 0; }

static int core_opts(const gsys_t *s, const char *path, core_opt_t *o)
{
	int n = 0;
	char label[160], b[64];
	/* the RetroAchievements core first when it's there: that's what a plain launch picks */
	char rarbf[96], setname[64];
	if (ra_core_for(s->id, rarbf, sizeof(rarbf), setname, sizeof(setname))) {
		snprintf(label, sizeof(label), "%s (RetroAchievements)", s->name);
		n = add_opt(o, n, rarbf, setname, label, s);
		snprintf(o[n - 1].kind, sizeof(o[0].kind), "ra");
	}
	/* the system's own, and its alternatives */
	char rbfs[96], *save = NULL;
	snprintf(rbfs, sizeof(rbfs), "%s", s->rbf);
	for (char *c = strtok_r(rbfs, "|", &save); c; c = strtok_r(NULL, "|", &save)) {
		if (!core_installed(c)) continue;
		base_name(c, b, sizeof(b));
		if (strchr(s->rbf, '|')) snprintf(label, sizeof(label), "%s (%s core)", s->name, b);
		else snprintf(label, sizeof(label), "%s", s->name);
		int first = n;
		n = add_opt(o, n, c, NULL, label, s);
		/* its dated files: the newest is the one above (MiSTer picks it); older ones kept on the card can be
		   chosen too (an MGL naming the dated file loads exactly that one) */
		char dir[96], vers[8][96];
		const char *sl = strrchr(c, '/');
		snprintf(dir, sizeof(dir), "%.*s", sl ? (int)(sl - c) : 0, c);
		int nv = rbf_versions(dir, b, vers, 8);
		if (n > first && nv > 0) rbf_date(vers[0], o[first].date, sizeof(o[0].date));
		for (int v = 1; v < nv && n < MAX_CORE_OPTS; v++) {
			char vr[96], dt[12];
			snprintf(vr, sizeof(vr), "%s/%s", dir, vers[v]);
			rbf_date(vers[v], dt, sizeof(dt));
			snprintf(label, sizeof(label), "%s (older version)", s->name);
			int k = n;
			n = add_opt(o, n, vr, NULL, label, s);
			if (n > k) { snprintf(o[k].kind, sizeof(o[0].kind), "version"); snprintf(o[k].date, sizeof(o[0].date), "%s", dt); }
		}
		/* a test build from the core's developer, when one's installed (_Unstable) */
		nv = rbf_versions("_Unstable", b, vers, 8);
		if (nv > 0 && n < MAX_CORE_OPTS) {
			char vr[96];
			snprintf(vr, sizeof(vr), "_Unstable/%s", vers[0]);
			snprintf(label, sizeof(label), "%s (experimental)", s->name);
			int k = n;
			n = add_opt(o, n, vr, NULL, label, s);
			if (n > k) { snprintf(o[k].kind, sizeof(o[0].kind), "unstable"); rbf_date(vers[0], o[k].date, sizeof(o[0].date)); }
		}
	}
	(void)path;
	/* the same system's core built another way: dual SDRAM builds (TheJesusFish's Dual RAM Console Cores)
	   and the alternative builds in _Other (ajgowans' Alt Cores: PSX2XCPU, N64_80MHz, ...) */
	int yc = vt_fpga_yc();
	snprintf(rbfs, sizeof(rbfs), "%s", s->rbf);
	save = NULL;
	for (char *c = strtok_r(rbfs, "|", &save); c && n < MAX_CORE_OPTS; c = strtok_r(NULL, "|", &save)) {
		base_name(c, b, sizeof(b));
		char vers[8][96], vr[96];
		int nv = rbf_versions(DUAL_RAM_DIR, b, vers, 8);
		if (nv > 0) {
			snprintf(vr, sizeof(vr), "%s/%s", DUAL_RAM_DIR, vers[0]);
			snprintf(label, sizeof(label), "%s (dual RAM)", s->name);
			int k = n;
			n = add_opt(o, n, vr, NULL, label, s);
			if (n > k) { snprintf(o[k].kind, sizeof(o[0].kind), "dualram"); rbf_date(vers[0], o[k].date, sizeof(o[0].date)); }
		}
		n = alt_opts(o, n, b, s, "_Other", 0);
		if (yc) {                                        /* (only when the FPGA makes S-Video / composite) */
			n = alt_opts(o, n, b, s, "_YCConsole", 1);
			n = alt_opts(o, n, b, s, "_YCComputer", 1);
		}
	}
	/* older versions last, so the other builds are never off the bottom of a short list (stable: the
	   first stays the one a plain launch uses) */
	for (int i = 1; i < n; i++)
		for (int j = i; j > 0 && kind_rank(o[j].kind) < kind_rank(o[j - 1].kind); j--) {
			core_opt_t t = o[j]; o[j] = o[j - 1]; o[j - 1] = t;
		}
	return n;
}

/* the core a system's games start on when nothing is chosen (Always start with this): its rbf */
static void core_pref_key(const gsys_t *s, char *k, size_t n) { snprintf(k, n, "core_pref.%s", s->id); }
static int core_pref(sqlite3 *db, const gsys_t *s, const core_opt_t *o, int n)
{
	char k[64], v[100];
	core_pref_key(s, k, sizeof(k));
	if (!db_meta_get(db, k, v, sizeof(v)) || !v[0]) return -1;
	for (int i = 0; i < n; i++) if (!strcmp(o[i].rbf, v)) return i;
	return -1;                                           /* (that core is gone: the usual one again) */
}
static const char *kind_desc(const char *kind)
{
	if (!strcmp(kind, "ra")) return ra_logged_in() ? "Unlocks achievements while you play. Phosphor's default."
	                                               : "Phosphor's default. Log in under Settings > RetroAchievements to unlock achievements.";
	if (!strcmp(kind, "version")) return "An earlier build still on the SD card.";
	if (!strcmp(kind, "unstable")) return "A newer test build from the core's developer. May have bugs.";
	if (!strcmp(kind, "dualram")) return "Built for a second SDRAM module: needs the dual RAM board.";
	if (!strcmp(kind, "alt")) return "Another build of this system's core from its developer.";
	if (!strcmp(kind, "yc")) return "Makes S-Video and composite itself, for a CRT on those inputs (needs a Y/C cable).";
	return "The standard core from MiSTer's own distribution.";
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
			char stem[64];
			if (rbf_stem(b, stem, sizeof(stem))) snprintf(b, sizeof(b), "%s", stem);   /* (no date) */
			cJSON_AddNumberToObject(x, "index", i);
			cJSON_AddStringToObject(x, "name", o[i].label);
			cJSON_AddStringToObject(x, "core", b);        /* the core's name as MiSTer shows it (CORENAME) */
			cJSON_AddStringToObject(x, "kind", o[i].kind);
			cJSON_AddStringToObject(x, "desc", kind_desc(o[i].kind));
			if (o[i].date[0]) cJSON_AddStringToObject(x, "date", o[i].date);
			if (i == core_pref(db, s, o, n)) cJSON_AddBoolToObject(x, "preferred", 1);
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
		if (core < 0) core = core_pref(db, s, o, n);    /* the one chosen with Always start with this */
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

/* every system whose core is installed (not arcade: its games are its cores), for the in-game menu's
   Change core: [{id, name, core}] */
cJSON *play_system_cores(void)
{
	cJSON *a = cJSON_CreateArray();
	for (int i = 0; i < NSYS; i++) {
		char rbf[96], b[96];
		if (!strcmp(SYS[i].id, "Arcade") || !system_core(&SYS[i], rbf, sizeof(rbf))) continue;
		base_name(rbf, b, sizeof(b));
		cJSON *x = cJSON_CreateObject();
		cJSON_AddStringToObject(x, "id", SYS[i].id);
		cJSON_AddStringToObject(x, "name", SYS[i].name);
		cJSON_AddStringToObject(x, "core", b);
		cJSON_AddItemToArray(a, x);
	}
	return a;
}

/* a system's core on its own, no game (its own menu loads one, as when it's picked in MiSTer's menu) */
int play_load_core(sqlite3 *db, const char *sys_id, char *err, size_t errn)
{
	const gsys_t *s = sys_id ? system_by_id(sys_id) : NULL;
	char rbf[96], file[800], cmd[900];
	if (!s || !strcmp(s->id, "Arcade")) { snprintf(err, errn, "Unknown system"); return -1; }
	if (!system_core(s, rbf, sizeof(rbf)) || !system_core_file(rbf, file, sizeof(file))) {
		snprintf(err, errn, "The %s core isn't installed", s->name);
		return -1;
	}
	music_stop();
	add_play_time(db);                       /* (the game left: its time) */
	snprintf(cmd, sizeof(cmd), "load_core %s\n", file);
	return mister_cmd(cmd, err, errn);
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

/* Always start with this: a game's system starts on that core from now on (core -1: back to the usual) */
int play_core_pref(sqlite3 *db, long long id, int core, char *err, size_t errn)
{
	const gsys_t *s;
	char path[1024], k[64];
	if (game_file(db, id, &s, path, sizeof(path), err, errn)) return -1;
	core_pref_key(s, k, sizeof(k));
	if (core < 0) { db_meta_set(db, k, ""); return 0; }
	core_opt_t o[MAX_CORE_OPTS];
	int n = core_opts(s, path, o);
	if (core >= n) { snprintf(err, errn, "That core isn't there any more"); return -1; }
	db_meta_set(db, k, o[core].rbf);
	plog("%s games start on %s from now on", s->id, o[core].rbf);
	return 0;
}

/* ---------- the Other and Utility cards: the cores in _Other and _Utility ----------
 * Everything MiSTer, Update All or the person puts there, newest file of each core, with a readable
 * name (EpochGalaxyII -> Epoch Galaxy II). Utility adds Update All's account and Link state, for
 * registering the SuperStation for supporter cores (Coin-Op Collection, JOTEGO...). */
static void nice_name(const char *base, char *out, size_t n)
{
	static const struct { const char *b, *name; } KNOWN[] = {
		{ "ADCTest", "ADC Test" }, { "InputTest", "Input Test" }, { "MiSTerLaggy", "MiSTer Laggy (input lag)" },
		{ "memtest", "Memory test (SDRAM)" }, { "Chip8", "Chip-8" }, { "GBMidi", "Game Boy MIDI" },
		{ "GenMidi", "Genesis MIDI" },
	};
	for (size_t i = 0; i < sizeof(KNOWN) / sizeof(KNOWN[0]); i++)
		if (!strcasecmp(base, KNOWN[i].b)) { snprintf(out, n, "%s", KNOWN[i].name); return; }
	size_t j = 0;
	for (size_t i = 0; base[i] && j + 2 < n; i++) {
		char c = base[i], pc = i ? base[i - 1] : 0;
		int up = c >= 'A' && c <= 'Z', lowprev = pc >= 'a' && pc <= 'z', digprev = pc >= '0' && pc <= '9';
		int dig = c >= '0' && c <= '9';
		if (i && ((up && (lowprev || digprev)) || (dig && !digprev && pc != '_'))) out[j++] = ' ';
		out[j++] = c == '_' ? ' ' : c;
	}
	out[j] = 0;
}

cJSON *play_folder_cores(const char *folder)
{
	cJSON *a = cJSON_CreateArray();
	if (!folder || (strcmp(folder, "_Other") && strcmp(folder, "_Utility"))) return a;
	char d[600];
	snprintf(d, sizeof(d), "%s/%s", p_fat(), folder);
	DIR *dd = opendir(d);
	if (!dd) return a;
	char seen[64][64];
	int ns = 0;
	struct dirent *e;
	while ((e = readdir(dd))) {
		size_t l = strlen(e->d_name);
		if (l < 5 || e->d_name[0] == '.' || strcasecmp(e->d_name + l - 4, ".rbf")) continue;
		char base[64];
		const char *u = strrchr(e->d_name, '_');
		size_t bl = u && strspn(u + 1, "0123456789") == 8 ? (size_t)(u - e->d_name) : l - 4;
		snprintf(base, sizeof(base), "%.*s", (int)bl, e->d_name);
		int dup = 0;
		for (int i = 0; i < ns; i++) if (!strcasecmp(seen[i], base)) dup = 1;
		if (dup || ns >= 64) continue;
		snprintf(seen[ns++], 64, "%s", base);
		char vers[8][96];
		int nv = rbf_versions(folder, base, vers, 8);
		char file[700], name[96], dt[12] = "";
		if (nv > 0) { snprintf(file, sizeof(file), "%s/%s.rbf", d, vers[0]); rbf_date(vers[0], dt, sizeof(dt)); }
		else snprintf(file, sizeof(file), "%s/%s", d, e->d_name);
		nice_name(base, name, sizeof(name));
		cJSON *x = cJSON_CreateObject();
		cJSON_AddStringToObject(x, "name", name);
		cJSON_AddStringToObject(x, "file", file);
		if (dt[0]) cJSON_AddStringToObject(x, "date", dt);
		cJSON_AddItemToArray(a, x);
	}
	closedir(dd);
	int n = cJSON_GetArraySize(a);                       /* by name */
	for (int i = 1; i < n; i++)
		for (int j = i; j > 0; j--) {
			cJSON *p = cJSON_GetArrayItem(a, j - 1), *q = cJSON_GetArrayItem(a, j);
			if (strcasecmp(cJSON_GetStringValue(cJSON_GetObjectItem(p, "name")), cJSON_GetStringValue(cJSON_GetObjectItem(q, "name"))) <= 0) break;
			cJSON *t = cJSON_DetachItemFromArray(a, j);
			cJSON_InsertItemInArray(a, j - 1, t);
		}
	return a;
}

/* a core from _Other or _Utility, on its own (its menu picks a file, as from MiSTer's menu) */
int play_run_core(sqlite3 *db, const char *file, char *err, size_t errn)
{
	char real[PATH_MAX], d1[600], d2[600];
	snprintf(d1, sizeof(d1), "%s/_Other/", p_fat());
	snprintf(d2, sizeof(d2), "%s/_Utility/", p_fat());
	if (!file || !realpath(file, real) || (strncmp(real, d1, strlen(d1)) && strncmp(real, d2, strlen(d2))) || !has_ext(real, "rbf")) {
		snprintf(err, errn, "Not a core in _Other or _Utility");
		return -1;
	}
	char cmd[PATH_MAX + 16];
	music_stop();
	add_play_time(db);
	snprintf(cmd, sizeof(cmd), "load_core %s\n", real);
	plog("run core %s", real);
	return mister_cmd(cmd, err, errn);
}
