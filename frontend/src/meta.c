/* Phosphor frontend: game details (box art, description, year, publisher...).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Phosphor keeps its own library, one folder per system, keyed by the game's file name
 * (without extension), so the same game has the same art wherever it lives (SD, USB, share):
 *   /media/fat/phosphor/library/<System>/<title>.png|.jpg   box art (scaled to fit when shown;
 *                                                           ~512 px tall keeps it quick to load)
 *   /media/fat/phosphor/library/<System>/<title>.txt        details and description:
 *       Name: ...  Year: ...  Developer: ...  Publisher: ...  Genre: ...  Players: ...
 *       (one per line, any order, all optional), then a blank line, then the description
 * Games folders are never written to.
 *
 * What other tools already put on the card is read as well, after Phosphor's own:
 *   - gamelist.xml in the system's games folder (EmulationStation format, from common scrapers);
 *     paths in it are relative to its folder
 *   - media/<title>.png|.jpg|.txt next to the games (Console Mode's layout; SS1 Tool fills it) */
#include "meta.h"
#include "log.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

typedef struct {
	char *key;      /* absolute game path */
	meta_t m;
} entry_t;

static entry_t *E;
static int NE, CAPE;
static meta_t found;     /* returned by meta_for */
static meta_t own;       /* from the Phosphor library, freed on the next lookup */
static char *found_img, *found_desc;
static char lib_dir[512];

/* ---------- a small reader for gamelist.xml ---------- */

static void unescape(char *s)
{
	char *o = s;
	while (*s) {
		if (*s == '&') {
			if (!strncmp(s, "&amp;", 5)) { *o++ = '&'; s += 5; continue; }
			if (!strncmp(s, "&lt;", 4)) { *o++ = '<'; s += 4; continue; }
			if (!strncmp(s, "&gt;", 4)) { *o++ = '>'; s += 4; continue; }
			if (!strncmp(s, "&quot;", 6)) { *o++ = '"'; s += 6; continue; }
			if (!strncmp(s, "&apos;", 6)) { *o++ = '\''; s += 6; continue; }
			if (!strncmp(s, "&#x", 3) || !strncmp(s, "&#", 2)) {
				int hex = s[2] == 'x';
				char *end;
				long c = strtol(s + (hex ? 3 : 2), &end, hex ? 16 : 10);
				if (*end == ';') {
					/* UTF-8 encode */
					if (c < 0x80) *o++ = (char)c;
					else if (c < 0x800) { *o++ = (char)(0xC0 | (c >> 6)); *o++ = (char)(0x80 | (c & 0x3F)); }
					else { *o++ = (char)(0xE0 | (c >> 12)); *o++ = (char)(0x80 | ((c >> 6) & 0x3F)); *o++ = (char)(0x80 | (c & 0x3F)); }
					s = end + 1;
					continue;
				}
			}
		}
		*o++ = *s++;
	}
	*o = 0;
}

/* the text of <tag>...</tag> inside [from, to), or NULL */
static char *tag_text(const char *from, const char *to, const char *tag)
{
	char open[32], close[32];
	snprintf(open, sizeof(open), "<%s>", tag);
	snprintf(close, sizeof(close), "</%s>", tag);
	const char *a = strstr(from, open);
	if (!a || a >= to) return NULL;
	a += strlen(open);
	const char *b = strstr(a, close);
	if (!b || b > to) return NULL;
	char *t = strndup(a, (size_t)(b - a));
	unescape(t);
	/* trim */
	char *s = t;
	while (isspace((unsigned char)*s)) s++;
	char *e = s + strlen(s);
	while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
	if (s != t) memmove(t, s, strlen(s) + 1);
	if (!*t) { free(t); return NULL; }
	return t;
}

static char *resolve(const char *dir, const char *rel)
{
	if (!rel) return NULL;
	char p[1024];
	if (rel[0] == '/') snprintf(p, sizeof(p), "%s", rel);
	else snprintf(p, sizeof(p), "%s/%s", dir, rel[0] == '.' && rel[1] == '/' ? rel + 2 : rel);
	return strdup(p);
}

static void add_entry(const char *dir, const char *b, const char *e)
{
	char *rel = tag_text(b, e, "path");
	if (!rel) return;
	if (NE == CAPE) { CAPE = CAPE ? CAPE * 2 : 128; E = realloc(E, sizeof(entry_t) * CAPE); }
	entry_t *x = &E[NE++];
	memset(x, 0, sizeof(*x));
	x->key = resolve(dir, rel);
	free(rel);
	x->m.name = tag_text(b, e, "name");
	x->m.desc = tag_text(b, e, "desc");
	char *img = tag_text(b, e, "image");
	if (!img) img = tag_text(b, e, "thumbnail");
	x->m.image = resolve(dir, img);
	free(img);
	char *d = tag_text(b, e, "releasedate");   /* 19970131T000000 -> 1997 */
	if (d && strlen(d) >= 4) d[4] = 0;
	x->m.date = d;
	x->m.developer = tag_text(b, e, "developer");
	x->m.publisher = tag_text(b, e, "publisher");
	x->m.genre = tag_text(b, e, "genre");
	x->m.players = tag_text(b, e, "players");
}

static void load_gamelist(const char *dir)
{
	char path[600];
	snprintf(path, sizeof(path), "%s/gamelist.xml", dir);
	FILE *f = fopen(path, "rb");
	if (!f) return;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n <= 0 || n > 64 * 1024 * 1024) { fclose(f); return; }
	char *buf = malloc((size_t)n + 1);
	size_t got = fread(buf, 1, (size_t)n, f);
	fclose(f);
	buf[got] = 0;
	int before = NE;
	for (char *p = buf; (p = strstr(p, "<game")) != NULL;) {
		if (p[5] != '>' && p[5] != ' ') { p += 5; continue; }
		char *end = strstr(p, "</game>");
		if (!end) break;
		add_entry(dir, p, end);
		p = end + 7;
	}
	free(buf);
	plog("gamelist: %d games in %s", NE - before, path);
}

static void free_meta(meta_t *m)
{
	free(m->name); free(m->desc); free(m->image); free(m->date);
	free(m->developer); free(m->publisher); free(m->genre); free(m->players);
}

void meta_free(void)
{
	for (int i = 0; i < NE; i++) { free(E[i].key); free_meta(&E[i].m); }
	free(E);
	E = NULL;
	NE = CAPE = 0;
}

void meta_load(const char *system_id, char dirs[][512], int ndirs)
{
	meta_free();
	const char *fat = getenv("PHOS_FAT");
	snprintf(lib_dir, sizeof(lib_dir), "%s/phosphor/library/%s", fat && *fat ? fat : "/media/fat", system_id);
	for (int i = 0; i < ndirs; i++) load_gamelist(dirs[i]);
}

/* ---------- lookup ---------- */

static char *read_text(const char *path);

static int exists(const char *p)
{
	struct stat st;
	return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

/* <games folder>/media/<title>.<ext>, looking in the game's own folder and up to the system folder */
static char *media_file(const char *game_path, const char *const *exts)
{
	char dir[1024], title[512];
	snprintf(dir, sizeof(dir), "%s", game_path);
	char *slash = strrchr(dir, '/');
	if (!slash) return NULL;
	snprintf(title, sizeof(title), "%s", slash + 1);
	char *dot = strrchr(title, '.');
	if (dot) *dot = 0;
	*slash = 0;
	for (int up = 0; up < 3; up++) {
		for (int i = 0; exts[i]; i++) {
			char p[1600];
			snprintf(p, sizeof(p), "%s/media/%s.%s", dir, title, exts[i]);
			if (exists(p)) return strdup(p);
		}
		char *s = strrchr(dir, '/');
		if (!s || !strcmp(s, "/games")) break;
		*s = 0;
	}
	return NULL;
}

static char *read_text(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;
	char *buf = malloc(8192);
	size_t n = fread(buf, 1, 8191, f);
	fclose(f);
	buf[n] = 0;
	return buf;
}

/* <title>.txt in the Phosphor library: "Key: value" lines, a blank line, then the description */
static void read_own(const char *path)
{
	char *t = read_text(path);
	if (!t) return;
	char *p = t;
	while (*p) {
		char *nl = strchr(p, '\n');
		if (nl) *nl = 0;
		char *line = p;
		size_t len = strlen(line);
		if (len && line[len - 1] == '\r') line[--len] = 0;
		p = nl ? nl + 1 : p + strlen(p);
		if (!len) break;                                  /* blank line: description follows */
		char *colon = strchr(line, ':');
		if (!colon) { p = line; if (nl) *nl = '\n'; break; }   /* no header: it's all description */
		*colon = 0;
		char *v = colon + 1;
		while (*v == ' ' || *v == '\t') v++;
		char **dst = !strcasecmp(line, "name") ? &own.name : !strcasecmp(line, "year") ? &own.date :
		             !strcasecmp(line, "developer") ? &own.developer : !strcasecmp(line, "publisher") ? &own.publisher :
		             !strcasecmp(line, "genre") ? &own.genre : !strcasecmp(line, "players") ? &own.players : NULL;
		if (dst && *v) { free(*dst); *dst = strdup(v); }
	}
	while (*p == '\n' || *p == '\r') p++;
	if (*p) own.desc = strdup(p);
	free(t);
}

static char *own_file(const char *game_path, const char *const *exts)
{
	const char *base = strrchr(game_path, '/');
	char title[512];
	snprintf(title, sizeof(title), "%s", base ? base + 1 : game_path);
	char *dot = strrchr(title, '.');
	if (dot) *dot = 0;
	for (int i = 0; exts[i]; i++) {
		char p[1100];
		snprintf(p, sizeof(p), "%s/%s.%s", lib_dir, title, exts[i]);
		if (exists(p)) return strdup(p);
	}
	return NULL;
}

#define PICK(field) (own.field ? own.field : x.field)

const meta_t *meta_for(const char *game_path)
{
	static const char *const img_ext[] = { "png", "jpg", "jpeg", "PNG", "JPG", NULL };
	static const char *const txt_ext[] = { "txt", NULL };
	free(found_img); found_img = NULL;
	free(found_desc); found_desc = NULL;
	free_meta(&own);
	memset(&own, 0, sizeof(own));
	memset(&found, 0, sizeof(found));

	/* 1. Phosphor's own library */
	char *t = own_file(game_path, txt_ext);
	if (t) { read_own(t); free(t); }
	own.image = own_file(game_path, img_ext);

	/* 2. gamelist.xml */
	meta_t x = { 0 };
	for (int i = 0; i < NE; i++) {
		if (!strcmp(E[i].key, game_path)) { x = E[i].m; break; }
	}
	if (x.image && !exists(x.image)) x.image = NULL;
	found.name = PICK(name); found.desc = PICK(desc); found.image = PICK(image); found.date = PICK(date);
	found.developer = PICK(developer); found.publisher = PICK(publisher); found.genre = PICK(genre);
	found.players = PICK(players);

	/* 3. Console Mode's media/ folders */
	if (!found.image) found.image = found_img = media_file(game_path, img_ext);
	if (!found.desc) {
		char *m = media_file(game_path, txt_ext);
		if (m) { found.desc = found_desc = read_text(m); free(m); }
	}
	return &found;
}
