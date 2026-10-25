/* phosphord: a disc in the dock's CD/DVD drive.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Phosphor's Main reads the drive (phosphor-main phosphor_disc.cpp: "disc" on its socket, API 5) with
 * the physical disc support merged from Main_MiSTer_Physical_Disc (Anime0t4ku, bonna79). This adds
 * what Phosphor knows: the game's name from your library (the same disc code) or from libretro's
 * lists, its box art (the library game's, else libretro's, fetched once and kept in data/disc_art),
 * and why a disc can't be played. A disc game is started the way upstream does it: an .mgl that loads
 * the stock core under an A0CD-* set name (the name tells Main's disc code the core is for the drive).
 *
 * What was decided (docs/discs.md): the disc only shows while it's in (Disc › In the drive); nothing
 * starts by itself; music CDs and DVDs aren't played yet; Play last is the last game from storage
 * unless a disc was played after it and is still in. */
#include "disc.h"
#include "art.h"
#include "db.h"
#include "log.h"
#include "net.h"
#include "paths.h"
#include "play.h"
#include "systems.h"
#include <ctype.h>
#include <dirent.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

int libretro_disc_name(sqlite3 *db, const char *sys, const char *serial, char *name, size_t n,
                       char urls[][1300], int max_urls, int *nurls);   /* src_libretro.c */

/* Main's .mgl names (the same as upstream's "_Physical Disc Cores") -> the core and the set name */
static const struct { const char *mgl, *rbf, *core, *setname; } MGL[] = {
	{ "PSX",             "_Console/PSX",          "PSX",          "A0CD-PSX" },
	{ "Saturn",          "_Console/Saturn",       "Saturn",       "A0CD-Saturn" },
	{ "MegaCD",          "_Console/MegaCD",       "MegaCD",       "A0CD-MegaCD" },
	{ "TurboGrafx16-CD", "_Console/TurboGrafx16", "TurboGrafx16", "A0CD-TurboGrafx16" },
	{ "NeoGeoCD",        "_Console/NeoGeo",       "NeoGeo",       "A0CD-NeoGeoCD" },
	{ "3DO",             "_Console/3DO",          "3DO",          "A0CD-3DO" },
	{ "CDi",             "_Console/CDi",          "CDi",          "A0CD-CDi" },
	{ "MDPlus",          "_Console/MegaDrive",    "MegaDrive",    "A0CD-MDPlus" },
	{ "SNES-MSU1",       "_Console/SNES",         "SNES",         "A0CD-SNES" },
};

/* Main's disc types that are systems in Phosphor's library (systems.c) */
static const char *lib_sys(const char *type)
{
	if (!strcmp(type, "psx")) return "PSX";
	if (!strcmp(type, "saturn")) return "Saturn";
	if (!strcmp(type, "megacd")) return "MegaCD";
	if (!strcmp(type, "pcecd")) return "TGFX16CD";
	return NULL;
}

static char *main_req(const char *line, int timeout_ms)
{
	const char *sp = getenv("PHOSPHOR_MAIN_SOCK");
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) return NULL;
	struct sockaddr_un a = { .sun_family = AF_UNIX };
	snprintf(a.sun_path, sizeof(a.sun_path), "%s", sp && *sp ? sp : "/tmp/phosphor-main.sock");
	char msg[64];
	int n = snprintf(msg, sizeof(msg), "%s\n", line);
	if (connect(fd, (struct sockaddr *)&a, sizeof(a)) < 0 || send(fd, msg, (size_t)n, MSG_NOSIGNAL) != n) { close(fd); return NULL; }
	char *buf = calloc(1, 1024);
	size_t len = 0;
	while (buf && len < 1023) {
		struct pollfd p = { fd, POLLIN, 0 };
		if (poll(&p, 1, timeout_ms) <= 0) break;
		ssize_t r = read(fd, buf + len, 1);
		if (r <= 0 || buf[len] == '\n') break;
		len++;
	}
	close(fd);
	if (buf) buf[len] = 0;
	if (!buf || !len) { free(buf); return NULL; }
	return buf;
}

static int core_installed(const char *core)
{
	char dir[300];
	snprintf(dir, sizeof(dir), "%s/_Console", p_fat());
	DIR *d = opendir(dir);
	if (!d) return 0;
	size_t l = strlen(core);
	int found = 0;
	struct dirent *e;
	while (!found && (e = readdir(d)))
		if (!strncasecmp(e->d_name, core, l) && e->d_name[l] == '_' && strcasestr(e->d_name, ".rbf")) found = 1;
	closedir(d);
	return found;
}

/* what's known about the disc in the drive now: kept while the same disc is in */
static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
static struct {
	char key[96];            /* type|serial|label: the disc this is about */
	char title[256];
	long long game_id;       /* in the library, else 0 */
	char art[700];           /* box art file, "" none (yet) */
	int fetching;
} k;

static void *fetch_art(void *arg)
{
	char *job = arg;                       /* key \t dest \t url \t url ... */
	char *key = job, *dest = strchr(key, '\t');
	if (dest) *dest++ = 0;
	char *urls = dest ? strchr(dest, '\t') : NULL;
	if (urls) *urls++ = 0;
	int ok = 0;
	for (char *u = urls; u && *u && !ok;) {
		char *nx = strchr(u, '\t');
		if (nx) *nx++ = 0;
		char tmp[760];
		snprintf(tmp, sizeof(tmp), "%s.part", dest);
		if (net_get_file(u, tmp) == 0 && rename(tmp, dest) == 0) ok = 1;
		else unlink(tmp);
		u = nx;
	}
	pthread_mutex_lock(&lk);
	if (!strcmp(k.key, key) && ok) snprintf(k.art, sizeof(k.art), "%s", dest);
	k.fetching = 0;
	pthread_mutex_unlock(&lk);
	if (ok) plog("disc: box art for %s", key);
	free(job);
	return NULL;
}

static void name_disc(sqlite3 *db, const char *type, const char *serial, const char *label, const char *key)
{
	char title[256] = "", art[700] = "";
	long long gid = 0;
	const char *sys = lib_sys(type);
	if (sys && *serial) {
		sqlite3_stmt *st;
		if (sqlite3_prepare_v2(db, "SELECT g.id, g.title, IFNULL(g.art_box, IFNULL((SELECT b.art_box FROM games b WHERE b.id=g.based_on),'')) FROM files f JOIN games g ON g.id=f.game_id "
		                           "WHERE f.serial=? AND g.system_id=? ORDER BY g.last_played DESC LIMIT 1", -1, &st, NULL) == SQLITE_OK) {
			sqlite3_bind_text(st, 1, serial, -1, SQLITE_STATIC);
			sqlite3_bind_text(st, 2, sys, -1, SQLITE_STATIC);
			if (sqlite3_step(st) == SQLITE_ROW) {
				gid = sqlite3_column_int64(st, 0);
				snprintf(title, sizeof(title), "%s", sqlite3_column_text(st, 1));
				const char *a = (const char *)sqlite3_column_text(st, 2);
				if (a && *a) {
					char abs[700];
					art_path(a, abs, sizeof(abs));
					if (file_exists(abs)) snprintf(art, sizeof(art), "%s", abs);
				}
			}
			sqlite3_finalize(st);
		}
	}
	char urls[3][1300];
	int nurls = 0;
	if (sys && *serial) {
		char official[400];
		if (libretro_disc_name(db, sys, serial, official, sizeof(official), urls, 3, &nurls) == 1 && !title[0]) {
			snprintf(title, sizeof(title), "%.255s", official);
			char *p = strstr(title, " (");          /* "Sky Runner (USA)" -> "Sky Runner" */
			if (p) *p = 0;
		}
	}
	if (!title[0] && *label) snprintf(title, sizeof(title), "%s", label);
	/* box art: the library game's, else libretro's (fetched once, kept by disc code) */
	char dest[700] = "";
	if (!art[0] && sys && *serial) {
		char dir[600], safe[64];
		snprintf(dir, sizeof(dir), "%s/disc_art", p_data());
		mkdirs(dir);
		size_t j = 0;
		for (const char *c = serial; *c && j + 1 < sizeof(safe); c++) safe[j++] = (char)(isalnum((unsigned char)*c) || *c == '-' ? *c : '_');
		safe[j] = 0;
		snprintf(dest, sizeof(dest), "%s/%s_%s.png", dir, sys, safe);
		if (file_exists(dest)) { snprintf(art, sizeof(art), "%s", dest); dest[0] = 0; }
	}
	pthread_mutex_lock(&lk);
	if (!strcmp(k.key, key)) {
		snprintf(k.title, sizeof(k.title), "%s", title);
		snprintf(k.art, sizeof(k.art), "%s", art);
		k.game_id = gid;
		if (dest[0] && nurls && !k.fetching) {
			size_t cap = strlen(key) + strlen(dest) + 3 * 1300 + 8;
			char *job = malloc(cap);
			if (job) {
				int o = snprintf(job, cap, "%s\t%s", key, dest);
				for (int i = 0; i < nurls; i++) o += snprintf(job + o, cap - (size_t)o, "\t%s", urls[i]);
				pthread_t t;
				if (pthread_create(&t, NULL, fetch_art, job) == 0) { pthread_detach(t); k.fetching = 1; }
				else free(job);
			}
		}
	}
	pthread_mutex_unlock(&lk);
}

static const char *reason(const char *state, const char *type, const char *mgl, int core_ok)
{
	if (!strcmp(state, "unreadable")) return "can't be read: clean it and try again";
	if (strcmp(state, "ready")) return "";
	if (!strcmp(type, "audio")) return "music CD: not supported yet";
	if (!strcmp(type, "dvd")) return "DVD: not supported";
	if (!*mgl) return "not a disc the SuperStation plays";
	if (!core_ok) return "its core isn't installed";
	return "";
}

static const char *mgl_core(const char *mgl)
{
	for (size_t i = 0; i < sizeof(MGL) / sizeof(MGL[0]); i++) if (!strcmp(MGL[i].mgl, mgl)) return MGL[i].core;
	return NULL;
}

/* Settings › Discs (TV and apps): off, Main doesn't watch or read the drive at all */
int disc_enabled(sqlite3 *db)
{
	char v[8];
	return strcmp(db_meta_get(db, "discs", v, sizeof(v)), "0") != 0;
}

int disc_set_enabled(sqlite3 *db, int on)
{
	db_meta_set(db, "discs", on ? "1" : "0");
	char *r = main_req(on ? "disc on" : "disc off", 1500);
	int ok = r && strstr(r, "\"ok\":true");
	free(r);
	plog("disc: discs %s", on ? "on" : "off");
	return ok ? 0 : -1;
}

cJSON *disc_json(sqlite3 *db)
{
	char *r = main_req("disc", 1500);
	cJSON *m = r ? cJSON_Parse(r) : NULL;
	free(r);
	if (!m || !cJSON_IsTrue(cJSON_GetObjectItem(m, "ok"))) { cJSON_Delete(m); return NULL; }
	const char *state = cJSON_GetStringValue(cJSON_GetObjectItem(m, "state"));
	const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(m, "type"));
	const char *serial = cJSON_GetStringValue(cJSON_GetObjectItem(m, "serial"));
	const char *label = cJSON_GetStringValue(cJSON_GetObjectItem(m, "label"));
	const char *mgl = cJSON_GetStringValue(cJSON_GetObjectItem(m, "mgl"));
	const char *msys = cJSON_GetStringValue(cJSON_GetObjectItem(m, "system"));
	state = state ? state : ""; type = type ? type : ""; serial = serial ? serial : ""; label = label ? label : "";
	mgl = mgl ? mgl : ""; msys = msys ? msys : "";

	cJSON *o = cJSON_CreateObject();
	cJSON_AddBoolToObject(o, "enabled", disc_enabled(db));
	cJSON_AddBoolToObject(o, "drive", cJSON_IsTrue(cJSON_GetObjectItem(m, "drive")));
	cJSON_AddStringToObject(o, "state", state);
	cJSON_AddStringToObject(o, "type", type);
	const char *ls = lib_sys(type);
	const gsys_t *g = ls ? system_by_id(ls) : NULL;
	cJSON_AddStringToObject(o, "system", g ? g->name : msys);
	cJSON_AddStringToObject(o, "serial", serial);
	if (!strcmp(state, "ready")) {
		char key[96];
		snprintf(key, sizeof(key), "%s|%s|%s", type, serial, label);
		pthread_mutex_lock(&lk);
		int fresh = strcmp(k.key, key) != 0;
		if (fresh) { memset(&k, 0, sizeof(k)); snprintf(k.key, sizeof(k.key), "%s", key); }
		pthread_mutex_unlock(&lk);
		if (fresh) name_disc(db, type, serial, label, key);
		pthread_mutex_lock(&lk);
		cJSON_AddStringToObject(o, "title", k.title[0] ? k.title : (*msys ? msys : "Disc"));
		if (k.game_id) cJSON_AddNumberToObject(o, "game_id", (double)k.game_id);
		if (k.art[0]) cJSON_AddStringToObject(o, "art", k.art);
		else cJSON_AddNullToObject(o, "art");
		pthread_mutex_unlock(&lk);
	}
	const char *core = mgl_core(mgl);
	int core_ok = core && core_installed(core);
	const char *why = reason(state, type, mgl, core_ok);
	cJSON_AddBoolToObject(o, "playable", !strcmp(state, "ready") && *mgl && core_ok);
	cJSON_AddStringToObject(o, "reason", why);
	cJSON_Delete(m);
	return o;
}

int disc_play(sqlite3 *db, char *err, size_t errn)
{
	char *r = main_req("disc", 1500);
	cJSON *m = r ? cJSON_Parse(r) : NULL;
	free(r);
	if (!m) { snprintf(err, errn, "Phosphor's Main isn't answering"); return -1; }
	const char *state = cJSON_GetStringValue(cJSON_GetObjectItem(m, "state"));
	const char *mgl = cJSON_GetStringValue(cJSON_GetObjectItem(m, "mgl"));
	const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(m, "type"));
	const char *serial = cJSON_GetStringValue(cJSON_GetObjectItem(m, "serial"));
	size_t i = 0;
	for (; mgl && i < sizeof(MGL) / sizeof(MGL[0]); i++) if (!strcmp(MGL[i].mgl, mgl)) break;
	if (!state || strcmp(state, "ready") || !mgl || !*mgl || i == sizeof(MGL) / sizeof(MGL[0])) {
		snprintf(err, errn, "There's no game disc in the drive");
		cJSON_Delete(m);
		return -1;
	}
	if (!core_installed(MGL[i].core)) {
		snprintf(err, errn, "The %s core isn't installed", MGL[i].core);
		cJSON_Delete(m);
		return -1;
	}
	char path[600], cmd[700];
	snprintf(path, sizeof(path), "%s/disc.mgl", p_tmp());
	FILE *f = fopen(path, "w");
	if (!f) { snprintf(err, errn, "couldn't write %s", path); cJSON_Delete(m); return -1; }
	fprintf(f, "<mistergamedescription>\n\t<rbf>%s</rbf>\n\t<setname same_dir=\"1\">%s</setname>\n</mistergamedescription>\n",
	        MGL[i].rbf, MGL[i].setname);
	fclose(f);
	snprintf(cmd, sizeof(cmd), "load_core %s\n", path);
	if (mister_cmd(cmd, err, errn) != 0) { cJSON_Delete(m); return -1; }

	/* remember it for Play last; its play time goes to the library game with the same disc code */
	cJSON *ld = cJSON_CreateObject();
	cJSON_AddStringToObject(ld, "type", type ? type : "");
	cJSON_AddStringToObject(ld, "serial", serial ? serial : "");
	cJSON_AddNumberToObject(ld, "at", (double)time(NULL));
	char *s = cJSON_PrintUnformatted(ld);
	if (s) { db_meta_set(db, "last_disc", s); free(s); }
	cJSON_Delete(ld);
	long long gid = 0;
	pthread_mutex_lock(&lk);
	gid = k.game_id;
	pthread_mutex_unlock(&lk);
	play_track(db, gid);
	plog("disc: playing %s %s", type ? type : "", serial ? serial : "");
	cJSON_Delete(m);
	return 0;
}

int disc_eject(char *err, size_t errn)
{
	char *r = main_req("disc eject", 3000);
	cJSON *m = r ? cJSON_Parse(r) : NULL;
	free(r);
	if (!m) { snprintf(err, errn, "Phosphor's Main isn't answering"); return -1; }
	int ok = cJSON_IsTrue(cJSON_GetObjectItem(m, "ok"));
	if (!ok) snprintf(err, errn, "%s", cJSON_GetStringValue(cJSON_GetObjectItem(m, "error")) ?: "The tray didn't open");
	cJSON_Delete(m);
	return ok ? 0 : -1;
}

int disc_last(sqlite3 *db, cJSON *o, long long last_game_played)
{
	char buf[300];
	if (!db_meta_get(db, "last_disc", buf, sizeof(buf))[0]) return 0;
	cJSON *ld = cJSON_Parse(buf);
	if (!ld) return 0;
	long long at = (long long)cJSON_GetNumberValue(cJSON_GetObjectItem(ld, "at"));
	const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(ld, "type"));
	const char *serial = cJSON_GetStringValue(cJSON_GetObjectItem(ld, "serial"));
	int used = 0;
	if (at > last_game_played && type) {
		cJSON *d = disc_json(db);
		const char *dt = d ? cJSON_GetStringValue(cJSON_GetObjectItem(d, "type")) : NULL;
		const char *ds = d ? cJSON_GetStringValue(cJSON_GetObjectItem(d, "serial")) : NULL;
		if (d && cJSON_IsTrue(cJSON_GetObjectItem(d, "playable")) && dt && !strcmp(dt, type) && ds && !strcmp(ds, serial ? serial : "")) {
			cJSON_AddItemToObject(o, "disc", d);
			used = 1;
		} else cJSON_Delete(d);
	}
	cJSON_Delete(ld);
	return used;
}
