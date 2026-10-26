/* phosphord: RetroAchievements (retroachievements.org) for Phosphor.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Achievements themselves run in Phosphor's Main (phosphor-main: odelot's RetroAchievements Main merged,
 * rcheevos by the RetroAchievements team) on odelot's RetroAchievements cores. This side:
 *   - the one-time login: the username and password go to RetroAchievements once (client API "login2"),
 *     and only the token that comes back is kept, in /media/fat/retroachievements.cfg, where Main reads it
 *   - the settings Main reads from that file (hardcore, pop-up corner), and the unlock sound
 *   - odelot's cores, from his GitHub releases, into _RA_Cores/Cores, only when asked
 *   - which core a game starts on (play.c): the RetroAchievements one while logged in, when it's there
 *   - the viewer (Settings > RetroAchievements > Your progress, the library's badges): with the user's
 *     own Web API key, each console's game list with its ROM hashes is fetched once, and the library is
 *     matched to it with rcheevos' own hashing (third_party/rcheevos, MIT); progress comes from one call
 * Every request names Phosphor (User-Agent "Phosphor/<version> (MiSTer)"), as RetroAchievements asks. */
#include "ra.h"
#include "db.h"
#include "log.h"
#include "net.h"
#include "paths.h"
#include "systems.h"
#include "version.h"
#include "rc_hash.h"
#include "rc_consoles.h"
#include <ctype.h>
#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define RA_SITE "https://retroachievements.org"
static char UA[96];

/* odelot's RetroAchievements cores (the same list as MiSTer Companion's), and the Phosphor systems on them */
typedef struct { const char *title, *repo; } racore_t;
static const racore_t CORES[] = {
	{ "NES", "odelot/NES_MiSTer" }, { "SNES", "odelot/SNES_MiSTer" }, { "Gameboy", "odelot/Gameboy_MiSTer" },
	{ "GBA", "odelot/GBA_MiSTer" }, { "N64", "odelot/N64_MiSTer" }, { "PSX", "odelot/PSX_MiSTer" },
	{ "MegaDrive", "odelot/MegaDrive_MiSTer" }, { "MegaCD", "odelot/MegaCD_MiSTer" }, { "SMS", "odelot/SMS_MiSTer" },
	{ "TurboGrafx16", "odelot/TurboGrafx16_MiSTer" }, { "Atari7800", "odelot/Atari7800_MiSTer" },
	{ "S32X", "odelot/S32X_MiSTer" }, { "NeoGeo", "odelot/NeoGeo_MiSTer" },
};
#define NCORES (int)(sizeof(CORES) / sizeof(CORES[0]))

/* Phosphor system -> RA core and console (by the file's extension where one system holds two consoles) */
static const char *sys_core(const char *sys)
{
	static const char *const M[][2] = {
		{ "NES", "NES" }, { "SNES", "SNES" }, { "GAMEBOY", "Gameboy" }, { "GBA", "GBA" }, { "N64", "N64" },
		{ "PSX", "PSX" }, { "Genesis", "MegaDrive" }, { "MegaCD", "MegaCD" }, { "SMS", "SMS" }, { "GameGear", "SMS" },
		{ "TGFX16", "TurboGrafx16" }, { "TGFX16CD", "TurboGrafx16" }, { "ATARI7800", "Atari7800" },
	};
	for (size_t i = 0; i < sizeof(M) / sizeof(M[0]); i++) if (!strcmp(M[i][0], sys)) return M[i][1];
	return NULL;
}

static int ext_is(const char *path, const char *e)
{
	const char *d = strrchr(path, '.');
	return d && !strcasecmp(d + 1, e);
}

static int sys_console(const char *sys, const char *path)
{
	if (!strcmp(sys, "NES")) return ext_is(path, "fds") ? RC_CONSOLE_FAMICOM_DISK_SYSTEM : RC_CONSOLE_NINTENDO;
	if (!strcmp(sys, "SNES")) return RC_CONSOLE_SUPER_NINTENDO;
	if (!strcmp(sys, "GAMEBOY")) return ext_is(path, "gbc") ? RC_CONSOLE_GAMEBOY_COLOR : RC_CONSOLE_GAMEBOY;
	if (!strcmp(sys, "GBA")) return RC_CONSOLE_GAMEBOY_ADVANCE;
	if (!strcmp(sys, "N64")) return RC_CONSOLE_NINTENDO_64;
	if (!strcmp(sys, "PSX")) return RC_CONSOLE_PLAYSTATION;
	if (!strcmp(sys, "Genesis")) return RC_CONSOLE_MEGA_DRIVE;
	if (!strcmp(sys, "MegaCD")) return RC_CONSOLE_SEGA_CD;
	if (!strcmp(sys, "SMS")) return RC_CONSOLE_MASTER_SYSTEM;
	if (!strcmp(sys, "GameGear")) return RC_CONSOLE_GAME_GEAR;
	if (!strcmp(sys, "TGFX16")) return RC_CONSOLE_PC_ENGINE;
	if (!strcmp(sys, "TGFX16CD")) return RC_CONSOLE_PC_ENGINE_CD;
	if (!strcmp(sys, "ATARI7800")) return ext_is(path, "a26") || ext_is(path, "bin") ? RC_CONSOLE_ATARI_2600 : RC_CONSOLE_ATARI_7800;
	return 0;
}

/* ---------- retroachievements.cfg (Main reads it; one key=value per line) ---------- */

static void cfg_path(char *out, size_t n) { snprintf(out, n, "%s/retroachievements.cfg", p_fat()); }

static int cfg_get(const char *key, char *out, size_t n)
{
	char p[600], line[600];
	out[0] = 0;
	cfg_path(p, sizeof(p));
	FILE *f = fopen(p, "r");
	if (!f) return 0;
	size_t kl = strlen(key);
	int found = 0;
	while (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\r\n")] = 0;
		char *k = line;
		while (*k == ' ' || *k == '\t') k++;
		if (strncasecmp(k, key, kl) || (k[kl] != '=' && k[kl] != ' ')) continue;
		char *v = strchr(k, '=');
		if (!v) continue;
		v++;
		while (*v == ' ' || *v == '\t') v++;
		snprintf(out, n, "%s", v);
		found = 1;
	}
	fclose(f);
	return found;
}

/* sets (or with val NULL removes) keys; the rest of the file stays as it is */
static int cfg_set(const char *const *keys, const char *const *vals, int nkeys)
{
	char p[600], tmp[620], line[600];
	cfg_path(p, sizeof(p));
	snprintf(tmp, sizeof(tmp), "%s.new", p);
	FILE *in = fopen(p, "r"), *out = fopen(tmp, "w");
	if (!out) { if (in) fclose(in); return -1; }
	int done[16] = { 0 };
	while (in && fgets(line, sizeof(line), in)) {
		const char *k = line;
		while (*k == ' ' || *k == '\t') k++;
		int hit = -1;
		for (int i = 0; i < nkeys; i++) {
			size_t kl = strlen(keys[i]);
			if (!strncasecmp(k, keys[i], kl) && (k[kl] == '=' || k[kl] == ' ')) hit = i;
		}
		if (hit < 0) { fputs(line, out); if (!strchr(line, '\n')) fputc('\n', out); continue; }
		if (!done[hit] && vals[hit]) fprintf(out, "%s=%s\n", keys[hit], vals[hit]);
		done[hit] = 1;
	}
	if (!in) fputs("# RetroAchievements (Phosphor: Settings > RetroAchievements). Read by Phosphor's Main.\n", out);
	for (int i = 0; i < nkeys; i++) if (!done[i] && vals[i]) fprintf(out, "%s=%s\n", keys[i], vals[i]);
	if (in) fclose(in);
	fflush(out);
	fsync(fileno(out));
	fclose(out);
	if (rename(tmp, p)) { unlink(tmp); return -1; }
	return 0;
}

static void key_path(char *out, size_t n) { snprintf(out, n, "%s/ra_webkey", p_config()); }

static int web_key(char *out, size_t n)
{
	char p[600];
	key_path(p, sizeof(p));
	out[0] = 0;
	FILE *f = fopen(p, "r");
	if (!f) return 0;
	if (!fgets(out, (int)n, f)) out[0] = 0;
	fclose(f);
	out[strcspn(out, "\r\n \t")] = 0;
	return out[0] != 0;
}

/* ---------- requests ---------- */

static cJSON *get_json(const char *url, int *res)
{
	char ua[140];
	snprintf(ua, sizeof(ua), "User-Agent: %s", UA);
	const char *h[] = { ua, NULL };
	char *out = NULL;
	int http = 0;
	int r = net_request("GET", url, h, NULL, &out, NULL, &http);
	if (res) *res = http >= 400 ? http : r;          /* 401: the Web API key wasn't accepted */
	cJSON *j = (r == 0 && out) ? cJSON_Parse(out) : NULL;
	free(out);
	return j;
}

/* ---------- login ---------- */

int ra_login(const char *user, const char *password, char *err, size_t errn)
{
	if (!user || !*user || !password || !*password) { snprintf(err, errn, "Type your username and password"); return -1; }
	char eu[300], ep[600], body[1000], ua[140];
	net_escape(user, eu, sizeof(eu));
	net_escape(password, ep, sizeof(ep));
	snprintf(body, sizeof(body), "r=login2&u=%s&p=%s", eu, ep);
	memset(ep, 0, sizeof(ep));
	snprintf(ua, sizeof(ua), "User-Agent: %s", UA);
	const char *h[] = { ua, "Content-Type: application/x-www-form-urlencoded", NULL };
	char *out = NULL;
	int http = 0;
	/* the body goes to curl on its standard input, never on a command line */
	int r = net_request("POST", RA_SITE "/dorequest.php", h, body, &out, NULL, &http);
	memset(body, 0, sizeof(body));
	cJSON *j = out ? cJSON_Parse(out) : NULL;
	free(out);
	if (!j) { snprintf(err, errn, r == -1 ? "No answer from RetroAchievements: is the network up?" : "RetroAchievements didn't answer properly"); return -1; }
	const char *tok = cJSON_GetStringValue(cJSON_GetObjectItem(j, "Token"));
	const char *name = cJSON_GetStringValue(cJSON_GetObjectItem(j, "User"));
	if (!cJSON_IsTrue(cJSON_GetObjectItem(j, "Success")) || !tok || !*tok) {
		const char *e = cJSON_GetStringValue(cJSON_GetObjectItem(j, "Error"));
		snprintf(err, errn, "%s", e && *e ? e : "Wrong username or password");
		cJSON_Delete(j);
		return -1;
	}
	const char *keys[] = { "username", "token", "password" };
	const char *vals[] = { name && *name ? name : user, tok, NULL };
	int w = cfg_set(keys, vals, 3);
	cJSON_Delete(j);
	if (w) { snprintf(err, errn, "Couldn't save the login on the SD card"); return -1; }
	plog("retroachievements: logged in as %s (token kept, no password)", vals[0]);
	return 0;
}

void ra_logout(void)
{
	const char *keys[] = { "token", "password" };
	const char *vals[] = { NULL, NULL };
	cfg_set(keys, vals, 2);
}

int ra_logged_in(void)
{
	char u[128], t[128];
	return cfg_get("username", u, sizeof(u)) && u[0] && cfg_get("token", t, sizeof(t)) && t[0];
}

/* ---------- settings ---------- */

static void sound_paths(char *on, char *off, size_t n)
{
	snprintf(on, n, "%s/achievement.wav", p_fat());
	snprintf(off, n, "%s/achievement.wav.off", p_fat());
}

int ra_set(cJSON *req, char *err, size_t errn)
{
	cJSON *hc = cJSON_GetObjectItem(req, "hardcore");
	const char *pos = cJSON_GetStringValue(cJSON_GetObjectItem(req, "popups"));
	cJSON *snd = cJSON_GetObjectItem(req, "sound");
	if (cJSON_IsBool(hc)) {
		const char *k[] = { "hardcore" }, *v[] = { cJSON_IsTrue(hc) ? "1" : "0" };
		if (cfg_set(k, v, 1)) { snprintf(err, errn, "Couldn't save it on the SD card"); return -1; }
	}
	if (pos) {
		if (strcmp(pos, "left") && strcmp(pos, "center") && strcmp(pos, "right")) { snprintf(err, errn, "popups: left, center or right"); return -1; }
		const char *k[] = { "popup_position" }, *v[] = { pos };
		if (cfg_set(k, v, 1)) { snprintf(err, errn, "Couldn't save it on the SD card"); return -1; }
	}
	if (cJSON_IsBool(snd)) {
		char on[600], off[600];
		sound_paths(on, off, sizeof(on));
		if (cJSON_IsTrue(snd) && file_exists(off)) rename(off, on);
		if (!cJSON_IsTrue(snd) && file_exists(on)) rename(on, off);
	}
	return 0;
}

int ra_set_key(const char *key, char *err, size_t errn)
{
	char p[600];
	key_path(p, sizeof(p));
	if (!key || !*key) { unlink(p); return 0; }
	for (const char *c = key; *c; c++) if (!isalnum((unsigned char)*c)) { snprintf(err, errn, "That doesn't look like a Web API key (letters and numbers only)"); return -1; }
	if (strlen(key) != 32) { snprintf(err, errn, "A Web API key is 32 letters and numbers: that's %d", (int)strlen(key)); return -1; }
	FILE *f = fopen(p, "w");
	if (!f) { snprintf(err, errn, "Couldn't save it on the SD card"); return -1; }
	fprintf(f, "%s\n", key);
	fclose(f);
	return 0;
}

/* ---------- odelot's cores ---------- */

static void core_dir(char *out, size_t n) { snprintf(out, n, "%s/_RA_Cores/Cores", p_fat()); }

static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
static struct { int busy, done, total; char msg[200], err[200]; } job;

static void ver_path(char *out, size_t n) { snprintf(out, n, "%s/ra_cores.json", p_config()); }

static cJSON *versions(void)
{
	char p[600];
	ver_path(p, sizeof(p));
	FILE *f = fopen(p, "r");
	if (!f) return cJSON_CreateObject();
	char buf[4096];
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = 0;
	cJSON *j = cJSON_Parse(buf);
	return cJSON_IsObject(j) ? j : (cJSON_Delete(j), cJSON_CreateObject());
}

static void ra_mkdirs(const char *path)
{
	char p[700];
	snprintf(p, sizeof(p), "%s", path);
	for (char *s = p + 1; *s; s++) if (*s == '/') { *s = 0; mkdir(p, 0755); *s = '/'; }
	mkdir(p, 0755);
}

static void *get_cores(void *arg)
{
	(void)arg;
	char dir[600];
	core_dir(dir, sizeof(dir));
	ra_mkdirs(dir);
	cJSON *vers = versions();
	int fails = 0;
	for (int i = 0; i < NCORES; i++) {
		pthread_mutex_lock(&lk);
		job.done = i;
		snprintf(job.msg, sizeof(job.msg), "Getting the %s core (%d of %d)…", CORES[i].title, i + 1, NCORES);
		pthread_mutex_unlock(&lk);
		char url[300];
		snprintf(url, sizeof(url), "https://api.github.com/repos/%s/releases/latest", CORES[i].repo);
		int r = 0;
		cJSON *rel = get_json(url, &r);
		const char *tag = rel ? cJSON_GetStringValue(cJSON_GetObjectItem(rel, "tag_name")) : NULL;
		const char *have = cJSON_GetStringValue(cJSON_GetObjectItem(vers, CORES[i].title));
		char dest[700];
		snprintf(dest, sizeof(dest), "%s/%s.rbf", dir, CORES[i].title);
		if (!tag) { fails++; cJSON_Delete(rel); continue; }
		if (have && !strcmp(have, tag) && file_exists(dest)) { cJSON_Delete(rel); continue; }   /* up to date */
		const char *rbf = NULL, *zip = NULL;
		cJSON *a;
		cJSON_ArrayForEach(a, cJSON_GetObjectItem(rel, "assets")) {
			const char *nm = cJSON_GetStringValue(cJSON_GetObjectItem(a, "name"));
			const char *u = cJSON_GetStringValue(cJSON_GetObjectItem(a, "browser_download_url"));
			if (!nm || !u) continue;
			if (ext_is(nm, "rbf") && !rbf) rbf = u;
			else if (ext_is(nm, "zip") && !zip) zip = u;
		}
		int ok = 0;
		if (rbf) ok = net_get_file(rbf, dest) == 0;
		else if (zip) {
			char z[700], cmd[1600];
			snprintf(z, sizeof(z), "%s/ra_core.zip", p_tmp());
			if (net_get_file(zip, z) == 0) {
				/* the first .rbf in the zip */
				snprintf(cmd, sizeof(cmd), "f=$(unzip -Z1 '%s' | grep -i '\\.rbf$' | head -1) && [ -n \"$f\" ] && unzip -p '%s' \"$f\" > '%s.part' && mv -f '%s.part' '%s'", z, z, dest, dest, dest);
				ok = system(cmd) == 0;
				unlink(z);
			}
		}
		if (ok) {
			cJSON_DeleteItemFromObject(vers, CORES[i].title);
			cJSON_AddStringToObject(vers, CORES[i].title, tag);
			plog("retroachievements: %s core %s", CORES[i].title, tag);
		} else fails++;
		cJSON_Delete(rel);
	}
	char vp[600];
	ver_path(vp, sizeof(vp));
	char *s = cJSON_Print(vers);
	FILE *f = s ? fopen(vp, "w") : NULL;
	if (f) { fputs(s, f); fclose(f); }
	free(s);
	cJSON_Delete(vers);
	pthread_mutex_lock(&lk);
	job.busy = 0;
	job.done = NCORES;
	snprintf(job.msg, sizeof(job.msg), fails ? "Done, but %d core%s couldn't be fetched (is the network up?)" : "The RetroAchievements cores are up to date", fails, fails == 1 ? "" : "s");
	pthread_mutex_unlock(&lk);
	return NULL;
}

int ra_get_cores(char *err, size_t errn)
{
	pthread_mutex_lock(&lk);
	if (job.busy) { pthread_mutex_unlock(&lk); return 0; }
	job.busy = 1; job.done = 0; job.total = NCORES; job.msg[0] = 0;
	pthread_mutex_unlock(&lk);
	pthread_t t;
	if (pthread_create(&t, NULL, get_cores, NULL)) { job.busy = 0; snprintf(err, errn, "couldn't start"); return -1; }
	pthread_detach(t);
	return 0;
}

int ra_core_for(const char *sys, char *rbf, size_t rn, char *setname, size_t sn)
{
	const char *c = sys_core(sys);
	if (!c || !ra_logged_in()) return 0;
	char dir[600], p[700];
	core_dir(dir, sizeof(dir));
	snprintf(p, sizeof(p), "%s/%s.rbf", dir, c);
	if (!file_exists(p)) return 0;
	snprintf(rbf, rn, "_RA_Cores/Cores/%s", c);
	snprintf(setname, sn, "RA_%s", c);
	return 1;
}

/* ---------- matching the library (the viewer) ---------- */

static struct { int busy; char msg[200]; time_t at; } match;

static int rc_hash_file(int console, const char *path, char hash[33])
{
	hash[0] = 0;
	return rc_hash_generate_from_file(hash, (uint32_t)console, path) ? 0 : -1;
}

/* RetroAchievements' game list for a console, with every hash: hash -> (id, title, achievements) */
static cJSON *console_list(int console, const char *key, int *http)
{
	*http = 0;
	char p[700];
	snprintf(p, sizeof(p), "%s/ra", p_data());
	ra_mkdirs(p);
	snprintf(p, sizeof(p), "%s/ra/games_%d.json", p_data(), console);
	struct stat sb;
	if (!stat(p, &sb) && time(NULL) - sb.st_mtime < 7 * 86400) {      /* a week is fresh enough */
		FILE *f = fopen(p, "r");
		if (f) {
			char *b = malloc((size_t)sb.st_size + 1);
			size_t n = b ? fread(b, 1, (size_t)sb.st_size, f) : 0;
			fclose(f);
			if (b) { b[n] = 0; cJSON *j = cJSON_Parse(b); free(b); if (j) return j; }
		}
	}
	char url[400];
	snprintf(url, sizeof(url), RA_SITE "/API/API_GetGameList.php?i=%d&h=1&f=1&y=%s", console, key);
	cJSON *j = get_json(url, http);
	if (!cJSON_IsArray(j)) { cJSON_Delete(j); return NULL; }
	char *s = cJSON_PrintUnformatted(j);
	FILE *f = s ? fopen(p, "w") : NULL;
	if (f) { fputs(s, f); fclose(f); }
	free(s);
	return j;
}

static void *match_all(void *arg)
{
	(void)arg;
	sqlite3 *db = db_open();
	char key[80];
	if (!db || !web_key(key, sizeof(key))) {
		pthread_mutex_lock(&lk); match.busy = 0; snprintf(match.msg, sizeof(match.msg), "Add your Web API key first"); pthread_mutex_unlock(&lk);
		if (db) sqlite3_close(db);
		return NULL;
	}
	/* each game's first file on a system RetroAchievements covers, not hashed yet */
	sqlite3_stmt *st;
	typedef struct { long long id; char sys[24], path[1024]; } it_t;
	it_t *items = NULL;
	int n = 0, cap = 0;
	if (sqlite3_prepare_v2(db, "SELECT g.id, g.system_id, (SELECT f.path FROM files f WHERE f.game_id=g.id AND f.online=1 ORDER BY f.id LIMIT 1) "
	                       "FROM games g LEFT JOIN ra_games r ON r.game_id=g.id WHERE r.game_id IS NULL", -1, &st, NULL) == SQLITE_OK) {
		while (sqlite3_step(st) == SQLITE_ROW) {
			const char *sys = (const char *)sqlite3_column_text(st, 1), *path = (const char *)sqlite3_column_text(st, 2);
			if (!sys || !path || !sys_core(sys)) continue;
			if (n == cap) { cap = cap ? cap * 2 : 256; it_t *ni = realloc(items, (size_t)cap * sizeof(*items)); if (!ni) break; items = ni; }
			items[n].id = sqlite3_column_int64(st, 0);
			snprintf(items[n].sys, sizeof(items[n].sys), "%s", sys);
			snprintf(items[n].path, sizeof(items[n].path), "%s", path);
			n++;
		}
		sqlite3_finalize(st);
	}
	/* lookups an earlier version recorded as "no achievements" when the list couldn't be had (no list was
	 * ever saved): asked again */
	{
		char dir[700];
		snprintf(dir, sizeof(dir), "%s/ra", p_data());
		DIR *d = opendir(dir);
		struct dirent *e;
		int lists_saved = 0;
		while (d && (e = readdir(d))) if (!strncmp(e->d_name, "games_", 6)) lists_saved++;
		if (d) closedir(d);
		if (!lists_saved) sqlite3_exec(db, "DELETE FROM ra_games WHERE ra_id=0", NULL, NULL, NULL);
	}
	cJSON *lists[128] = { 0 };
	int tried[128] = { 0 };
	int found = 0, bad_key = 0, no_list = 0;
	for (int i = 0; i < n; i++) {
		pthread_mutex_lock(&lk);
		snprintf(match.msg, sizeof(match.msg), "Matching your library: %d of %d", i + 1, n);
		pthread_mutex_unlock(&lk);
		int c = sys_console(items[i].sys, items[i].path);
		if (c <= 0 || c >= 128) continue;
		if (!lists[c] && !tried[c]) {
			int http = 0;
			tried[c] = 1;
			lists[c] = console_list(c, key, &http);
			if (http == 401 || http == 403) { bad_key = 1; break; }
		}
		if (!lists[c]) { no_list++; continue; }   /* not recorded: asked again next time */
		char h[33];
		long long ra_id = 0;
		int tot = 0;
		if (lists[c] && rc_hash_file(c, items[i].path, h) == 0) {
			cJSON *g;
			cJSON_ArrayForEach(g, lists[c]) {
				cJSON *hs = cJSON_GetObjectItem(g, "Hashes"), *x;
				cJSON_ArrayForEach(x, hs) if (cJSON_IsString(x) && !strcasecmp(x->valuestring, h)) {
					ra_id = (long long)cJSON_GetNumberValue(cJSON_GetObjectItem(g, "ID"));
					tot = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(g, "NumAchievements"));
				}
				if (ra_id) break;
			}
		} else h[0] = 0;
		if (ra_id) found++;
		if (sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO ra_games(game_id, hash, ra_id, total, unlocked, hc, checked) VALUES(?,?,?,?,0,0,?)", -1, &st, NULL) == SQLITE_OK) {
			sqlite3_bind_int64(st, 1, items[i].id);
			sqlite3_bind_text(st, 2, h, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(st, 3, ra_id);
			sqlite3_bind_int(st, 4, tot);
			sqlite3_bind_int64(st, 5, (sqlite3_int64)time(NULL));
			sqlite3_step(st);
			sqlite3_finalize(st);
		}
	}
	for (int c = 0; c < 128; c++) cJSON_Delete(lists[c]);
	free(items);
	sqlite3_close(db);
	plog("retroachievements: %d of %d games matched%s%s", found, n, bad_key ? " (the Web API key wasn't accepted)" : "", no_list ? " (some lists couldn't be had)" : "");
	pthread_mutex_lock(&lk);
	match.busy = 0;
	match.at = time(NULL);
	if (bad_key) snprintf(match.msg, sizeof(match.msg), "RetroAchievements didn't accept your Web API key: check it on retroachievements.org (Settings, Keys) and enter it again");
	else if (no_list) snprintf(match.msg, sizeof(match.msg), "%d game%s matched; RetroAchievements couldn't be reached for the rest, try again later", found, found == 1 ? "" : "s");
	else snprintf(match.msg, sizeof(match.msg), n ? "%d more game%s matched" : "Your library is matched", found, found == 1 ? "" : "s");
	pthread_mutex_unlock(&lk);
	return NULL;
}

int ra_match(char *err, size_t errn)
{
	pthread_mutex_lock(&lk);
	if (match.busy) { pthread_mutex_unlock(&lk); return 0; }
	match.busy = 1;
	pthread_mutex_unlock(&lk);
	pthread_t t;
	if (pthread_create(&t, NULL, match_all, NULL)) { match.busy = 0; snprintf(err, errn, "couldn't start"); return -1; }
	pthread_detach(t);
	return 0;
}

/* the user's progress for every game they've played: updates the library's badges, and is the viewer */
cJSON *ra_profile(sqlite3 *db, char *err, size_t errn)
{
	char key[80], user[128];
	if (!cfg_get("username", user, sizeof(user)) || !user[0]) { snprintf(err, errn, "Log in to RetroAchievements first"); return NULL; }
	if (!web_key(key, sizeof(key))) { snprintf(err, errn, "Add your Web API key first"); return NULL; }
	char eu[300], url[600];
	net_escape(user, eu, sizeof(eu));
	snprintf(url, sizeof(url), RA_SITE "/API/API_GetUserSummary.php?u=%s&g=0&a=0&y=%s", eu, key);
	int r = 0;
	cJSON *sum = get_json(url, &r);
	if (!sum) { snprintf(err, errn, r == 2 ? "RetroAchievements refused the Web API key" : "No answer from RetroAchievements"); return NULL; }
	snprintf(url, sizeof(url), RA_SITE "/API/API_GetUserCompletionProgress.php?u=%s&c=500&y=%s", eu, key);
	cJSON *prog = get_json(url, &r);
	cJSON *o = cJSON_CreateObject();
	cJSON_AddStringToObject(o, "user", user);
	cJSON_AddNumberToObject(o, "points", cJSON_GetNumberValue(cJSON_GetObjectItem(sum, "TotalPoints")));
	cJSON_AddNumberToObject(o, "softcore_points", cJSON_GetNumberValue(cJSON_GetObjectItem(sum, "TotalSoftcorePoints")));
	cJSON *rank = cJSON_GetObjectItem(sum, "Rank");
	if (cJSON_IsNumber(rank)) cJSON_AddNumberToObject(o, "rank", rank->valuedouble);
	const char *pic = cJSON_GetStringValue(cJSON_GetObjectItem(sum, "UserPic"));
	if (pic) { char u[300]; snprintf(u, sizeof(u), "https://media.retroachievements.org%s", pic); cJSON_AddStringToObject(o, "picture", u); }
	cJSON *games = cJSON_AddArrayToObject(o, "games");
	cJSON *g;
	int mastered = 0, achievements = 0;
	cJSON_ArrayForEach(g, cJSON_GetObjectItem(prog, "Results")) {
		long long id = (long long)cJSON_GetNumberValue(cJSON_GetObjectItem(g, "GameID"));
		int tot = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(g, "MaxPossible"));
		int un = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(g, "NumAwarded"));
		int hc = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(g, "NumAwardedHardcore"));
		const char *award = cJSON_GetStringValue(cJSON_GetObjectItem(g, "HighestAwardKind"));
		achievements += un;
		if (award && (!strcmp(award, "mastered") || !strcmp(award, "completed"))) mastered++;
		cJSON *x = cJSON_CreateObject();
		cJSON_AddNumberToObject(x, "id", (double)id);
		const char *t = cJSON_GetStringValue(cJSON_GetObjectItem(g, "Title"));
		const char *cn = cJSON_GetStringValue(cJSON_GetObjectItem(g, "ConsoleName"));
		const char *ic = cJSON_GetStringValue(cJSON_GetObjectItem(g, "ImageIcon"));
		cJSON_AddStringToObject(x, "title", t ? t : "");
		cJSON_AddStringToObject(x, "console", cn ? cn : "");
		if (ic) { char u[300]; snprintf(u, sizeof(u), "https://media.retroachievements.org%s", ic); cJSON_AddStringToObject(x, "icon", u); }
		cJSON_AddNumberToObject(x, "total", tot);
		cJSON_AddNumberToObject(x, "unlocked", un);
		cJSON_AddNumberToObject(x, "hardcore", hc);
		if (award) cJSON_AddStringToObject(x, "award", award);
		/* in the library? (matched by hash) — and its badge there */
		sqlite3_stmt *st;
		if (db && sqlite3_prepare_v2(db, "UPDATE ra_games SET total=?, unlocked=?, hc=? WHERE ra_id=?", -1, &st, NULL) == SQLITE_OK) {
			sqlite3_bind_int(st, 1, tot); sqlite3_bind_int(st, 2, un); sqlite3_bind_int(st, 3, hc); sqlite3_bind_int64(st, 4, id);
			sqlite3_step(st); sqlite3_finalize(st);
		}
		if (db && sqlite3_prepare_v2(db, "SELECT game_id FROM ra_games WHERE ra_id=? LIMIT 1", -1, &st, NULL) == SQLITE_OK) {
			sqlite3_bind_int64(st, 1, id);
			if (sqlite3_step(st) == SQLITE_ROW) cJSON_AddNumberToObject(x, "game_id", (double)sqlite3_column_int64(st, 0));
			sqlite3_finalize(st);
		}
		cJSON_AddItemToArray(games, x);
	}
	cJSON_AddNumberToObject(o, "achievements", achievements);
	cJSON_AddNumberToObject(o, "mastered", mastered);
	cJSON_Delete(sum);
	cJSON_Delete(prog);
	return o;
}

/* one library game: its achievements (the game page's list) */
cJSON *ra_game(sqlite3 *db, long long game_id, char *err, size_t errn)
{
	char key[80], user[128];
	if (!web_key(key, sizeof(key))) { snprintf(err, errn, "Add your Web API key first"); return NULL; }
	cfg_get("username", user, sizeof(user));
	sqlite3_stmt *st;
	long long ra_id = -1;
	if (sqlite3_prepare_v2(db, "SELECT ra_id FROM ra_games WHERE game_id=?", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(st, 1, game_id);
		if (sqlite3_step(st) == SQLITE_ROW) ra_id = sqlite3_column_int64(st, 0);
		sqlite3_finalize(st);
	}
	if (ra_id < 0) { snprintf(err, errn, "Not matched yet: Settings > RetroAchievements > Match my library"); return NULL; }
	cJSON *o = cJSON_CreateObject();
	cJSON_AddNumberToObject(o, "ra_id", (double)ra_id);
	if (!ra_id) { cJSON_AddBoolToObject(o, "supported", 0); return o; }
	cJSON_AddBoolToObject(o, "supported", 1);
	char eu[300], url[600];
	net_escape(user, eu, sizeof(eu));
	snprintf(url, sizeof(url), RA_SITE "/API/API_GetGameInfoAndUserProgress.php?g=%lld&u=%s&y=%s", ra_id, eu, key);
	int r = 0;
	cJSON *g = get_json(url, &r);
	if (!g) { cJSON_Delete(o); snprintf(err, errn, "No answer from RetroAchievements"); return NULL; }
	int tot = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(g, "NumAchievements"));
	int un = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(g, "NumAwardedToUser"));
	int hc = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(g, "NumAwardedToUserHardcore"));
	cJSON_AddNumberToObject(o, "total", tot);
	cJSON_AddNumberToObject(o, "unlocked", un);
	cJSON_AddNumberToObject(o, "hardcore", hc);
	cJSON *list = cJSON_AddArrayToObject(o, "achievements"), *a;
	int pts = 0, pts_un = 0;
	cJSON_ArrayForEach(a, cJSON_GetObjectItem(g, "Achievements")) {
		cJSON *x = cJSON_CreateObject();
		const char *t = cJSON_GetStringValue(cJSON_GetObjectItem(a, "Title"));
		const char *d = cJSON_GetStringValue(cJSON_GetObjectItem(a, "Description"));
		const char *b = cJSON_GetStringValue(cJSON_GetObjectItem(a, "BadgeName"));
		int p = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(a, "Points"));
		int got = cJSON_GetStringValue(cJSON_GetObjectItem(a, "DateEarned")) != NULL;
		pts += p; if (got) pts_un += p;
		cJSON_AddNumberToObject(x, "id", cJSON_GetNumberValue(cJSON_GetObjectItem(a, "ID")));
		cJSON_AddStringToObject(x, "title", t ? t : "");
		cJSON_AddStringToObject(x, "description", d ? d : "");
		cJSON_AddNumberToObject(x, "points", p);
		cJSON_AddBoolToObject(x, "unlocked", got);
		cJSON_AddBoolToObject(x, "hardcore", cJSON_GetStringValue(cJSON_GetObjectItem(a, "DateEarnedHardcore")) != NULL);
		if (b) { char u[200]; snprintf(u, sizeof(u), "https://media.retroachievements.org/Badge/%s%s.png", b, got ? "" : "_lock"); cJSON_AddStringToObject(x, "badge", u); }
		cJSON_AddItemToArray(list, x);
	}
	cJSON_AddNumberToObject(o, "points", pts);
	cJSON_AddNumberToObject(o, "points_unlocked", pts_un);
	if (sqlite3_prepare_v2(db, "UPDATE ra_games SET total=?, unlocked=?, hc=? WHERE game_id=?", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_int(st, 1, tot); sqlite3_bind_int(st, 2, un); sqlite3_bind_int(st, 3, hc); sqlite3_bind_int64(st, 4, game_id);
		sqlite3_step(st); sqlite3_finalize(st);
	}
	cJSON_Delete(g);
	return o;
}

/* the badge for a game in lists: {"ra":{"total":40,"unlocked":12}} when known */
void ra_badge(sqlite3 *db, long long game_id, cJSON *g)
{
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, "SELECT ra_id, total, unlocked FROM ra_games WHERE game_id=?", -1, &st, NULL) != SQLITE_OK) return;
	sqlite3_bind_int64(st, 1, game_id);
	if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_int64(st, 0) > 0 && sqlite3_column_int(st, 1) > 0) {
		cJSON *r = cJSON_AddObjectToObject(g, "ra");
		cJSON_AddNumberToObject(r, "total", sqlite3_column_int(st, 1));
		cJSON_AddNumberToObject(r, "unlocked", sqlite3_column_int(st, 2));
	}
	sqlite3_finalize(st);
}

cJSON *ra_status(void)
{
	cJSON *o = cJSON_CreateObject();
	char u[128], v[64], k[80], on[600], off[600];
	cfg_get("username", u, sizeof(u));
	cJSON_AddStringToObject(o, "user", u);
	cJSON_AddBoolToObject(o, "logged_in", ra_logged_in());
	cJSON_AddBoolToObject(o, "hardcore", cfg_get("hardcore", v, sizeof(v)) && atoi(v));
	cJSON_AddStringToObject(o, "popups", cfg_get("popup_position", v, sizeof(v)) && v[0] ? v : "left");
	sound_paths(on, off, sizeof(on));
	cJSON_AddBoolToObject(o, "sound", file_exists(on));
	cJSON_AddBoolToObject(o, "web_key", web_key(k, sizeof(k)));
	cJSON *vers = versions(), *cs = cJSON_AddArrayToObject(o, "cores");
	char dir[600];
	core_dir(dir, sizeof(dir));
	int have = 0;
	for (int i = 0; i < NCORES; i++) {
		char p[700];
		snprintf(p, sizeof(p), "%s/%s.rbf", dir, CORES[i].title);
		cJSON *x = cJSON_CreateObject();
		cJSON_AddStringToObject(x, "core", CORES[i].title);
		int in = file_exists(p);
		have += in;
		cJSON_AddBoolToObject(x, "installed", in);
		const char *tv = cJSON_GetStringValue(cJSON_GetObjectItem(vers, CORES[i].title));
		if (tv) cJSON_AddStringToObject(x, "version", tv);
		cJSON_AddItemToArray(cs, x);
	}
	cJSON_Delete(vers);
	cJSON_AddNumberToObject(o, "cores_installed", have);
	pthread_mutex_lock(&lk);
	cJSON *j = cJSON_AddObjectToObject(o, "job");
	cJSON_AddBoolToObject(j, "busy", job.busy);
	cJSON_AddStringToObject(j, "message", job.msg);
	cJSON *m = cJSON_AddObjectToObject(o, "match");
	cJSON_AddBoolToObject(m, "busy", match.busy);
	cJSON_AddStringToObject(m, "message", match.msg);
	pthread_mutex_unlock(&lk);
	return o;
}

void ra_init(void)
{
	snprintf(UA, sizeof(UA), "Phosphor/%s (MiSTer)", PHOSD_VERSION);
}
