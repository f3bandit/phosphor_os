/* Phosphor frontend: game library (systems, folders, scanning) and launching.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Games live where MiSTer keeps them: <root>/games/<System>/..., with roots on the SD card,
 * USB/NVMe drives and the network share. Arcade games are the .mra files in /media/fat/_Arcade.
 * A game is started the way MiSTer's own tools do it: an MGL file handed to Main through
 * /dev/MiSTer_cmd ("load_core <file>"); arcade .mra files are loaded directly.
 *
 * Paths can be redirected for tests: PHOS_FAT (default /media/fat), PHOS_GAMES_ROOTS
 * (| separated), PHOS_MISTER_CMD (default /dev/MiSTer_cmd). */
#include "library.h"
#include "log.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

game_system_t *SYSTEMS;
int NSYSTEMS;

static volatile int scan_gen;
static pthread_mutex_t scan_lock = PTHREAD_MUTEX_INITIALIZER;

static const char *CAT_NAMES[CAT_COUNT] = { "Disc", "Cartridge", "Handheld", "Arcade", "Computer" };
const char *category_name(category_t c) { return c < CAT_COUNT ? CAT_NAMES[c] : "?"; }

/* Built-in table. The MGL slot numbers (type/index/delay) follow each core's file menu and are
 * collected in systems.ini so they can be corrected without rebuilding. */
static const game_system_t BUILTIN[] = {
	/* id          name                   category       folders                rbf                                exts                  t  i  d */
	{ "PSX",       "PlayStation",         CAT_DISC,      "PSX",                 "_Console/PSX",                    "cue|chd|exe",        's', 1, 1, -1, 0 },
	{ "Saturn",    "Saturn",              CAT_DISC,      "Saturn",              "_Console/Saturn",                 "cue|chd",            's', 0, 1, -1, 0 },
	{ "MegaCD",    "Mega-CD / Sega CD",   CAT_DISC,      "MegaCD",              "_Console/MegaCD",                 "cue|chd",            's', 0, 1, -1, 0 },
	{ "TGFX16CD",  "PC Engine CD",        CAT_DISC,      "TGFX16-CD",           "_Console/TurboGrafx16",           "cue|chd",            's', 0, 1, -1, 0 },
	{ "NES",       "NES / Famicom",       CAT_CARTRIDGE, "NES",                 "_Console/NES",                    "nes|fds|nsf",        'f', 1, 1, -1, 0 },
	{ "SNES",      "Super NES",           CAT_CARTRIDGE, "SNES",                "_Console/SNES",                   "sfc|smc",            'f', 0, 1, -1, 0 },
	{ "Genesis",   "Mega Drive / Genesis",CAT_CARTRIDGE, "MegaDrive|Genesis",   "_Console/MegaDrive|_Console/Genesis","md|gen|bin|smd",   'f', 1, 1, -1, 0 },
	{ "N64",       "Nintendo 64",         CAT_CARTRIDGE, "N64",                 "_Console/N64",                    "n64|z64|v64",        'f', 1, 1, -1, 0 },
	{ "SMS",       "Master System",       CAT_CARTRIDGE, "SMS",                 "_Console/SMS",                    "sms|sg",             'f', 1, 1, -1, 0 },
	{ "TGFX16",    "PC Engine",           CAT_CARTRIDGE, "TGFX16",              "_Console/TurboGrafx16",           "pce|sgx",            'f', 1, 1, -1, 0 },
	{ "ATARI7800", "Atari 7800 / 2600",   CAT_CARTRIDGE, "ATARI7800|ATARI2600", "_Console/Atari7800",              "a78|a26|bin",        'f', 1, 1, -1, 0 },
	{ "GAMEBOY",   "Game Boy / Color",    CAT_HANDHELD,  "GAMEBOY|GBC",         "_Console/Gameboy",                "gb|gbc",             'f', 1, 1, -1, 0 },
	{ "GBA",       "Game Boy Advance",    CAT_HANDHELD,  "GBA",                 "_Console/GBA",                    "gba",                'f', 1, 1, -1, 0 },
	{ "GameGear",  "Game Gear",           CAT_HANDHELD,  "GameGear",            "_Console/SMS",                    "gg",                 'f', 2, 1, -1, 0 },
	{ "WonderSwan","WonderSwan",          CAT_HANDHELD,  "WonderSwan",          "_Console/WonderSwan",             "ws|wsc",             'f', 1, 1, -1, 0 },
	{ "AtariLynx", "Atari Lynx",          CAT_HANDHELD,  "AtariLynx",           "_Console/AtariLynx",              "lnx",                'f', 1, 1, -1, 0 },
	{ "Arcade",    "Arcade",              CAT_ARCADE,    "",                    "",                                "mra",                'f', 0, 0, -1, 0 },
	{ "C64",       "Commodore 64",        CAT_COMPUTER,  "C64",                 "_Computer/C64",                   "prg|crt|d64|t64",    'f', 1, 1, -1, 0 },
	{ "MSX",       "MSX",                 CAT_COMPUTER,  "MSX",                 "_Computer/MSX",                   "rom|dsk",            'f', 1, 1, -1, 0 },
	{ "Atari800",  "Atari 800",           CAT_COMPUTER,  "ATARI800",            "_Computer/Atari800",              "atr|xex|car|rom",    'f', 1, 1, -1, 0 },
};

static const char *fat(void)
{
	const char *f = getenv("PHOS_FAT");
	return f && *f ? f : "/media/fat";
}

/* ---------- systems.ini (optional overrides and extra systems) ---------- */

static game_system_t *find_system(const char *id)
{
	for (int i = 0; i < NSYSTEMS; i++)
		if (!strcasecmp(SYSTEMS[i].id, id)) return &SYSTEMS[i];
	return NULL;
}

static char *trim(char *s)
{
	while (isspace((unsigned char)*s)) s++;
	char *e = s + strlen(s);
	while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
	if (*s == '"' && e > s + 1 && e[-1] == '"') { e[-1] = 0; s++; }
	return s;
}

static void load_systems_ini(void)
{
	char path[512];
	snprintf(path, sizeof(path), "%s/phosphor/config/systems.ini", fat());
	FILE *f = fopen(path, "r");
	if (!f) return;
	char line[512];
	game_system_t *cur = NULL;
	while (fgets(line, sizeof(line), f)) {
		char *s = trim(line);
		if (!*s || *s == ';' || *s == '#') continue;
		if (*s == '[') {
			char *e = strchr(s, ']');
			if (!e) continue;
			*e = 0;
			cur = find_system(s + 1);
			if (!cur) {
				SYSTEMS = realloc(SYSTEMS, sizeof(*SYSTEMS) * (NSYSTEMS + 1));
				cur = &SYSTEMS[NSYSTEMS++];
				memset(cur, 0, sizeof(*cur));
				snprintf(cur->id, sizeof(cur->id), "%s", s + 1);
				snprintf(cur->name, sizeof(cur->name), "%s", s + 1);
				cur->cat = CAT_COMPUTER; cur->type = 'f'; cur->index = 1; cur->delay = 1; cur->count = -1;
			}
			continue;
		}
		char *eq = strchr(s, '=');
		if (!cur || !eq) continue;
		*eq = 0;
		char *k = trim(s), *v = trim(eq + 1);
		if (!strcasecmp(k, "name")) snprintf(cur->name, sizeof(cur->name), "%s", v);
		else if (!strcasecmp(k, "folders")) snprintf(cur->folders, sizeof(cur->folders), "%s", v);
		else if (!strcasecmp(k, "rbf")) snprintf(cur->rbf, sizeof(cur->rbf), "%s", v);
		else if (!strcasecmp(k, "extensions")) snprintf(cur->exts, sizeof(cur->exts), "%s", v);
		else if (!strcasecmp(k, "type")) cur->type = (char)tolower((unsigned char)v[0]);
		else if (!strcasecmp(k, "index")) cur->index = atoi(v);
		else if (!strcasecmp(k, "delay")) cur->delay = atoi(v);
		else if (!strcasecmp(k, "category")) {
			for (int c = 0; c < CAT_COUNT; c++)
				if (!strcasecmp(v, CAT_NAMES[c])) cur->cat = (category_t)c;
		}
	}
	fclose(f);
	plog("loaded %s", path);
}

void library_init(void)
{
	NSYSTEMS = (int)(sizeof(BUILTIN) / sizeof(BUILTIN[0]));
	SYSTEMS = malloc(sizeof(BUILTIN));
	memcpy(SYSTEMS, BUILTIN, sizeof(BUILTIN));
	load_systems_ini();
}

/* ---------- roots and scanning ---------- */

static int roots(char out[][256], int max)
{
	int n = 0;
	const char *env = getenv("PHOS_GAMES_ROOTS");
	if (env && *env) {
		char buf[1024];
		snprintf(buf, sizeof(buf), "%s", env);
		char *save = NULL;
		for (char *t = strtok_r(buf, "|", &save); t && n < max; t = strtok_r(NULL, "|", &save)) snprintf(out[n++], 256, "%s", t);
		return n;
	}
	/* same order MiSTer itself uses: USB drives first, then the share, then the SD card */
	for (int i = 0; i < 8 && n < max; i++) snprintf(out[n++], 256, "/media/usb%d/games", i);
	snprintf(out[n++], 256, "%s/cifs/games", fat());
	snprintf(out[n++], 256, "%s/games", fat());
	return n;
}

static int has_ext(const char *name, const char *exts)
{
	const char *dot = strrchr(name, '.');
	if (!dot || !dot[1]) return 0;
	size_t el = strlen(dot + 1);
	for (const char *p = exts; *p;) {
		const char *bar = strchr(p, '|');
		size_t l = bar ? (size_t)(bar - p) : strlen(p);
		if (l == el && !strncasecmp(p, dot + 1, l)) return 1;
		if (!bar) break;
		p = bar + 1;
	}
	return 0;
}

typedef void (*visit_fn)(const char *path, const char *name, void *ctx);

/* walk a folder (and subfolders, 3 deep), calling fn for matching files */
static void walk(const char *dir, const char *exts, int depth, visit_fn fn, void *ctx)
{
	DIR *d = opendir(dir);
	if (!d) return;
	struct dirent *e;
	char path[1024];
	while ((e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
		int is_dir = e->d_type == DT_DIR;
		if (e->d_type == DT_UNKNOWN || e->d_type == DT_LNK) {
			struct stat st;
			if (stat(path, &st) == 0) is_dir = S_ISDIR(st.st_mode);
		}
		if (is_dir) {
			if (depth < 3) walk(path, exts, depth + 1, fn, ctx);
		} else if (has_ext(e->d_name, exts)) {
			fn(path, e->d_name, ctx);
		}
	}
	closedir(d);
}

/* each existing folder for this system across the roots */
static int system_dirs(const game_system_t *s, char out[][512], int max)
{
	int n = 0;
	if (!strcmp(s->id, "Arcade")) {
		snprintf(out[n++], 512, "%s/_Arcade", fat());
		return n;
	}
	char r[12][256];
	int nr = roots(r, 12);
	for (int i = 0; i < nr && n < max; i++) {
		char folders[96];
		snprintf(folders, sizeof(folders), "%s", s->folders);
		char *save = NULL;
		for (char *f = strtok_r(folders, "|", &save); f && n < max; f = strtok_r(NULL, "|", &save)) {
			char p[512];
			snprintf(p, sizeof(p), "%s/%s", r[i], f);
			struct stat st;
			if (stat(p, &st) == 0 && S_ISDIR(st.st_mode)) snprintf(out[n++], 512, "%s", p);
		}
	}
	return n;
}

static void count_cb(const char *path, const char *name, void *ctx)
{
	(void)path; (void)name;
	(*(int *)ctx)++;
}

static int core_installed(const game_system_t *s)
{
	if (!strcmp(s->id, "Arcade")) return 1;
	char rbfs[96];
	snprintf(rbfs, sizeof(rbfs), "%s", s->rbf);
	char *save = NULL;
	for (char *r = strtok_r(rbfs, "|", &save); r; r = strtok_r(NULL, "|", &save)) {
		char dir[512], base[128];
		const char *slash = strrchr(r, '/');
		snprintf(dir, sizeof(dir), "%s/%.*s", fat(), slash ? (int)(slash - r) : 0, r);
		snprintf(base, sizeof(base), "%s", slash ? slash + 1 : r);
		DIR *d = opendir(dir);
		if (!d) continue;
		struct dirent *e;
		size_t bl = strlen(base);
		int found = 0;
		while ((e = readdir(d)) && !found) {
			/* PSX_20260801.rbf or PSX.rbf */
			if (!strncasecmp(e->d_name, base, bl) && (e->d_name[bl] == '_' || e->d_name[bl] == '.') && has_ext(e->d_name, "rbf"))
				found = 1;
		}
		closedir(d);
		if (found) return 1;
	}
	return 0;
}

static void *scan_thread(void *arg)
{
	(void)arg;
	for (int i = 0; i < NSYSTEMS; i++) {
		char dirs[16][512];
		int nd = system_dirs(&SYSTEMS[i], dirs, 16), count = 0;
		for (int j = 0; j < nd; j++) walk(dirs[j], SYSTEMS[i].exts, 0, count_cb, &count);
		int core = core_installed(&SYSTEMS[i]);
		pthread_mutex_lock(&scan_lock);
		SYSTEMS[i].count = count;
		SYSTEMS[i].core_found = core;
		pthread_mutex_unlock(&scan_lock);
		__sync_add_and_fetch(&scan_gen, 1);
	}
	plog("library scan done");
	return NULL;
}

void library_scan_start(void)
{
	pthread_t t;
	if (pthread_create(&t, NULL, scan_thread, NULL) == 0) pthread_detach(t);
}

int library_scan_generation(void) { return scan_gen; }

int library_category_count(category_t c)
{
	int n = 0, any = 0;
	pthread_mutex_lock(&scan_lock);
	for (int i = 0; i < NSYSTEMS; i++)
		if (SYSTEMS[i].cat == c && SYSTEMS[i].count >= 0) { n += SYSTEMS[i].count; any = 1; }
	pthread_mutex_unlock(&scan_lock);
	return any ? n : -1;
}

int library_category_systems(category_t c, int *out, int max)
{
	int n = 0;
	pthread_mutex_lock(&scan_lock);
	for (int i = 0; i < NSYSTEMS && n < max; i++)
		if (SYSTEMS[i].cat == c && SYSTEMS[i].count != 0) out[n++] = i;   /* hide systems with no games */
	pthread_mutex_unlock(&scan_lock);
	return n;
}

/* ---------- game lists ---------- */

typedef struct {
	game_t *g;
	int n, cap;
} glist_t;

static void add_cb(const char *path, const char *name, void *ctx)
{
	glist_t *l = ctx;
	if (l->n == l->cap) {
		l->cap = l->cap ? l->cap * 2 : 256;
		l->g = realloc(l->g, sizeof(game_t) * l->cap);
	}
	l->g[l->n].path = strdup(path);
	char *t = strdup(name);
	char *dot = strrchr(t, '.');
	if (dot) *dot = 0;
	l->g[l->n].title = t;
	l->n++;
}

/* natural order: "Quest 2" before "Quest 10"; case doesn't matter */
static int natcmp(const char *a, const char *b)
{
	while (*a && *b) {
		if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
			while (*a == '0') a++;
			while (*b == '0') b++;
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

static int by_title(const void *a, const void *b)
{
	return natcmp(((const game_t *)a)->title, ((const game_t *)b)->title);
}

game_t *library_games(const game_system_t *s, int *n)
{
	glist_t l = { 0 };
	char dirs[16][512];
	int nd = system_dirs(s, dirs, 16);
	for (int j = 0; j < nd; j++) walk(dirs[j], s->exts, 0, add_cb, &l);
	if (l.n > 1) qsort(l.g, l.n, sizeof(game_t), by_title);
	*n = l.n;
	return l.g;
}

void library_free_games(game_t *g, int n)
{
	for (int i = 0; i < n; i++) { free(g[i].path); free(g[i].title); }
	free(g);
}

/* ---------- launching ---------- */

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

int mister_cmd(const char *cmd, char *err, size_t errn)
{
	const char *dev = getenv("PHOS_MISTER_CMD");
	if (!dev || !*dev) dev = "/dev/MiSTer_cmd";
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
	plog("sent: %s", cmd);
	return 0;
}

int library_launch(const game_system_t *s, const game_t *g, char *err, size_t errn)
{
	char cmd[1200];
	if (!strcmp(s->id, "Arcade")) {
		snprintf(cmd, sizeof(cmd), "load_core %s\n", g->path);
		return mister_cmd(cmd, err, errn);
	}
	if (!s->core_found) {
		snprintf(err, errn, "The %s core isn't installed", s->name);
		return -1;
	}
	/* the first rbf name that exists */
	char rbfs[96], rbf[96] = "";
	snprintf(rbfs, sizeof(rbfs), "%s", s->rbf);
	char *save = NULL;
	for (char *r = strtok_r(rbfs, "|", &save); r; r = strtok_r(NULL, "|", &save)) {
		game_system_t one = *s;
		snprintf(one.rbf, sizeof(one.rbf), "%s", r);
		if (core_installed(&one)) { snprintf(rbf, sizeof(rbf), "%s", r); break; }
	}
	char esc[1024];
	xml_escape(esc, sizeof(esc), g->path);
	const char *mgl = "/tmp/phosphor_launch.mgl";
	FILE *f = fopen(mgl, "w");
	if (!f) {
		snprintf(err, errn, "couldn't write %s", mgl);
		return -1;
	}
	fprintf(f, "<mistergamedescription>\n\t<rbf>%s</rbf>\n\t<file delay=\"%d\" type=\"%c\" index=\"%d\" path=\"%s\"/>\n</mistergamedescription>\n",
	        rbf, s->delay, s->type, s->index, esc);
	fclose(f);
	snprintf(cmd, sizeof(cmd), "load_core %s\n", mgl);
	return mister_cmd(cmd, err, errn);
}
