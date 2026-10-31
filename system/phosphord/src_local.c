/* phosphord: the "Local files" source: box art and details already on the drives, no network.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Asked like any other source, first by default (it's instant and offline). Nothing on the drives is
 * changed: pictures are copied into Phosphor's store, scaled down, as from any source. For a game, in
 * this order (a field's first answer wins, as everywhere):
 *
 *  1. gamelist.xml (EmulationStation, Skraper) in the game's folder or one above it, up to the system
 *     folder: the entry for this very file: its <image> (or <thumbnail>) and its details.
 *  2. media/ folders next to the games, or one or two folders up: media/box2d, covers, boxart,
 *     boxFront, media/ itself, box3d, images. The picture named like the game: the same name first,
 *     then the same title words (brackets, region and year left out: "Gamename.png" for
 *     "Gamename (USA) (1999).sfc"), the one sharing the most bracket tags (region) when there are
 *     several. media/<title>.txt is the description, as Console Mode keeps it.
 *  3. The MiSTer artwork packs Update All installs (chipster6502's): docs/<games folder>/Artwork on
 *     the SD card or a USB drive, matched as their PACK_FORMAT.md says: the file name, then index.tsv
 *     by name, a trailing tag that is itself a key, the CRC and size, then the bare title when only one
 *     game has it. gameinfo.tsv and synopsis_*.tsv give the details. Game Boy and Game Boy Color share
 *     theirs; .fds files try FDS before NES.
 *
 * Folders are read once a run (local_reset() at the start of each), so 12,000 games don't mean
 * 12,000 directory listings. Screenshots, title screens and logos are left alone: Phosphor keeps box
 * art only. */
#include "scrape_src.h"
#include "ident.h"
#include "importer.h"
#include "log.h"
#include "paths.h"
#include "scraper.h"
#include "systems.h"
#include <ctype.h>
#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;   /* the SS1's own run and the PC app's asks */

static int is_file(const char *p)
{
	struct stat st;
	return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

static int is_dir(const char *p)
{
	struct stat st;
	return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

/* the bracket tags of a name, lower case, "|usa|1999|" */
static void tags_of(const char *name, char *out, size_t n)
{
	size_t o = 0;
	out[0] = 0;
	if (n < 3) return;
	out[o++] = '|';
	for (const char *p = name; *p; p++) {
		if (*p != '(' && *p != '[') continue;
		char close = *p == '(' ? ')' : ']';
		const char *e = strchr(p + 1, close);
		if (!e) break;
		/* "(USA, Europe)": each one */
		const char *a = p + 1;
		while (a < e) {
			const char *c = memchr(a, ',', (size_t)(e - a));
			if (!c) c = e;
			while (a < c && *a == ' ') a++;
			for (const char *q = a; q < c && o + 2 < n; q++) out[o++] = (char)tolower((unsigned char)*q);
			if (o + 2 < n) out[o++] = '|';
			a = c + 1;
		}
		p = e;
	}
	out[o] = 0;
}

static int shared_tags(const char *a, const char *b)
{
	int n = 0;
	char t[64];
	for (const char *p = a + 1; *p;) {
		const char *e = strchr(p, '|');
		if (!e) break;
		if (e - p > 0 && e - p < (int)sizeof(t) - 2) {
			snprintf(t, sizeof(t), "|%.*s|", (int)(e - p), p);
			if (strstr(b, t)) n++;
		}
		p = e + 1;
	}
	return n;
}

/* ---------- a folder of pictures, read once ---------- */

typedef struct { char *file, *base, *norm, *tags; } lent_t;
typedef struct { char *path; lent_t *e; int n; } ldir_t;

#define MAX_DIRS 256
static ldir_t dirs[MAX_DIRS];
static int ndirs;

static int is_image(const char *ext)
{
	static const char *const EX[] = { "png", "jpg", "jpeg", "gif", "bmp", NULL };
	for (int i = 0; EX[i]; i++) if (!strcasecmp(ext, EX[i])) return 1;
	return 0;
}

/* call with mu held; NULL if the folder isn't there */
static ldir_t *dir_read(const char *path)
{
	for (int i = 0; i < ndirs; i++)
		if (!strcmp(dirs[i].path, path)) {
			/* most recently used last: the shared folders (games/PSX/media/box2d) stay, the one-off ones go */
			if (i != ndirs - 1) {
				ldir_t t = dirs[i];
				memmove(&dirs[i], &dirs[i + 1], (size_t)(ndirs - 1 - i) * sizeof(dirs[0]));
				dirs[ndirs - 1] = t;
			}
			ldir_t *d = &dirs[ndirs - 1];
			return d->e || d->n == 0 ? d : NULL;
		}
	ldir_t d = { strdup(path), NULL, 0 };
	DIR *dp = opendir(path);
	int cap = 0;
	if (dp) {
		struct dirent *de;
		while ((de = readdir(dp)) != NULL) {
			if (de->d_name[0] == '.') continue;
			const char *dot = strrchr(de->d_name, '.');
			if (!dot || !is_image(dot + 1)) continue;
			if (d.n == cap) {
				cap = cap ? cap * 2 : 64;
				lent_t *ne = realloc(d.e, (size_t)cap * sizeof(*ne));
				if (!ne) break;
				d.e = ne;
			}
			lent_t *e = &d.e[d.n];
			e->file = strdup(de->d_name);
			e->base = strndup(de->d_name, (size_t)(dot - de->d_name));
			char nb[512], tb[256];
			title_norm(e->base ? e->base : "", nb, sizeof(nb));
			tags_of(e->base ? e->base : "", tb, sizeof(tb));
			e->norm = strdup(nb);
			e->tags = strdup(tb);
			if (!e->file || !e->base || !e->norm || !e->tags) { free(e->file); free(e->base); free(e->norm); free(e->tags); break; }
			d.n++;
		}
		closedir(dp);
	}
	if (ndirs == MAX_DIRS) { /* full: the oldest goes */
		for (int k = 0; k < dirs[0].n; k++) { free(dirs[0].e[k].file); free(dirs[0].e[k].base); free(dirs[0].e[k].norm); free(dirs[0].e[k].tags); }
		free(dirs[0].e);
		free(dirs[0].path);
		memmove(dirs, dirs + 1, (MAX_DIRS - 1) * sizeof(dirs[0]));
		ndirs--;
	}
	if (!dp) { free(d.e); d.e = NULL; d.n = -1; }   /* remembered as missing */
	dirs[ndirs++] = d;
	return dp ? &dirs[ndirs - 1] : NULL;
}

/* the picture in a folder for a game name: the same name, else the same title words (most shared
   tags, then the shortest name). out: the full path. 1 found */
static int dir_find(const char *dir, const char *name, char *out, size_t n)
{
	ldir_t *d = dir_read(dir);
	if (!d || d->n <= 0) return 0;
	for (int i = 0; i < d->n; i++)
		if (!strcasecmp(d->e[i].base, name)) { snprintf(out, n, "%s/%s", dir, d->e[i].file); return 1; }
	char nb[512], tb[256];
	title_norm(name, nb, sizeof(nb));
	if (!nb[0]) return 0;
	tags_of(name, tb, sizeof(tb));
	int best = -1, best_score = -1;
	size_t best_len = 0;
	for (int i = 0; i < d->n; i++) {
		if (strcmp(d->e[i].norm, nb)) continue;
		int sc = shared_tags(tb, d->e[i].tags);
		size_t len = strlen(d->e[i].base);
		if (sc > best_score || (sc == best_score && len < best_len)) { best = i; best_score = sc; best_len = len; }
	}
	if (best < 0) return 0;
	snprintf(out, n, "%s/%s", dir, d->e[best].file);
	return 1;
}

/* ---------- gamelist.xml, read once a folder ---------- */

typedef struct { char *path, *image, *f[F_COUNT]; } glent_t;
typedef struct { char *dir; glent_t *e; int n; } glist_t;

#define MAX_LISTS 64
static glist_t lists[MAX_LISTS];
static int nlists;

static int gl_cmp(const void *a, const void *b) { return strcmp(((const glent_t *)a)->path, ((const glent_t *)b)->path); }

static glist_t *list_read(const char *dir)
{
	for (int i = 0; i < nlists; i++) if (!strcmp(lists[i].dir, dir)) return &lists[i];
	if (nlists == MAX_LISTS) {           /* full (a folder a game: discs): the oldest goes */
		glist_t *o = &lists[0];
		for (int k = 0; k < o->n; k++) { free(o->e[k].path); free(o->e[k].image); for (int f = 0; f < F_COUNT; f++) free(o->e[k].f[f]); }
		free(o->e);
		free(o->dir);
		memmove(lists, lists + 1, (MAX_LISTS - 1) * sizeof(lists[0]));
		nlists--;
	}
	glist_t *L = &lists[nlists++];
	memset(L, 0, sizeof(*L));
	L->dir = strdup(dir);
	char path[1100];
	snprintf(path, sizeof(path), "%s/gamelist.xml", dir);
	FILE *f = fopen(path, "rb");
	if (!f) return L;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz <= 0 || sz > 64L * 1024 * 1024) { fclose(f); return L; }
	char *buf = malloc((size_t)sz + 1);
	size_t got = buf ? fread(buf, 1, (size_t)sz, f) : 0;
	fclose(f);
	if (!buf) return L;
	buf[got] = 0;
	int cap = 0;
	for (char *p = buf; (p = strstr(p, "<game")) != NULL;) {
		if (p[5] != '>' && p[5] != ' ') { p += 5; continue; }
		char *end = strstr(p, "</game>");
		if (!end) break;
		char *rel = gl_tag_text(p, end, "path");
		if (rel) {
			if (L->n == cap) {
				cap = cap ? cap * 2 : 256;
				glent_t *ne = realloc(L->e, (size_t)cap * sizeof(*ne));
				if (!ne) { free(rel); break; }
				L->e = ne;
			}
			glent_t *e = &L->e[L->n++];
			memset(e, 0, sizeof(*e));
			char gp[1100];
			gl_resolve(dir, rel, gp, sizeof(gp));
			if (!(e->path = strdup(gp))) { L->n--; free(rel); break; }
			char *img = gl_tag_text(p, end, "image");
			if (!img) img = gl_tag_text(p, end, "thumbnail");
			if (img) { char ip[1100]; gl_resolve(dir, img, ip, sizeof(ip)); e->image = strdup(ip); free(img); }
			e->f[F_DESC] = gl_tag_text(p, end, "desc");
			e->f[F_DEV] = gl_tag_text(p, end, "developer");
			e->f[F_PUB] = gl_tag_text(p, end, "publisher");
			e->f[F_GENRE] = gl_tag_text(p, end, "genre");
			e->f[F_PLAYERS] = gl_tag_text(p, end, "players");
			char *date = gl_tag_text(p, end, "releasedate");
			if (date && strlen(date) >= 4 && isdigit((unsigned char)date[0])) { date[4] = 0; e->f[F_YEAR] = date; }
			else free(date);
			free(rel);
		}
		p = end + 7;
	}
	free(buf);
	if (L->n > 1) qsort(L->e, (size_t)L->n, sizeof(*L->e), gl_cmp);
	plog("local: %s (%d entries)", path, L->n);
	return L;
}

static glent_t *list_find(glist_t *L, const char *path)
{
	if (!L || L->n <= 0) return NULL;
	glent_t key = { .path = (char *)path };
	return bsearch(&key, L->e, (size_t)L->n, sizeof(*L->e), gl_cmp);
}

/* ---------- the artwork packs (docs/<System>/Artwork) ---------- */

typedef struct { char *a, *b; } kv_t;          /* lookup key -> value */
typedef struct { char *key, *f[F_COUNT]; } info_t;
typedef struct {
	char *dir;                                  /* .../docs/<System>/Artwork, or NULL: none */
	kv_t *by_name, *by_crc, *by_norm;           /* index.tsv: lower-case name / "crc:size" / title words -> key */
	int nname, ncrc, nnorm;
	info_t *info;                               /* gameinfo.tsv + synopsis, sorted by key */
	int ninfo;
} pack_t;

#define MAX_PACKS 48
static pack_t packs[MAX_PACKS];
static char pack_folder[MAX_PACKS][32];
static int npacks;

static int kv_cmp(const void *a, const void *b) { return strcmp(((const kv_t *)a)->a, ((const kv_t *)b)->a); }
static int info_cmp(const void *a, const void *b) { return strcmp(((const info_t *)a)->key, ((const info_t *)b)->key); }

static void kv_add(kv_t **arr, int *n, int *cap, const char *a, const char *b)
{
	if (*n == *cap) {
		int nc = *cap ? *cap * 2 : 1024;
		kv_t *na = realloc(*arr, (size_t)nc * sizeof(**arr));
		if (!na) return;
		*arr = na;
		*cap = nc;
	}
	char *x = strdup(a), *y = strdup(b);
	if (!x || !y) { free(x); free(y); return; }
	(*arr)[*n].a = x;
	(*arr)[(*n)++].b = y;
}

static const char *kv_get(kv_t *arr, int n, const char *a)
{
	if (!arr || n <= 0) return NULL;
	kv_t k = { .a = (char *)a };
	kv_t *r = bsearch(&k, arr, (size_t)n, sizeof(*arr), kv_cmp);
	return r ? r->b : NULL;
}

static void lower(char *s) { for (; *s; s++) *s = (char)tolower((unsigned char)*s); }

/* one TSV line's fields (tabs replaced by NULs); returns how many */
static int tsv_split(char *line, char **f, int max)
{
	line[strcspn(line, "\r\n")] = 0;
	int n = 0;
	char *p = line;
	while (n < max) {
		f[n++] = p;
		char *t = strchr(p, '\t');
		if (!t) break;
		*t = 0;
		p = t + 1;
	}
	return n;
}

static void pack_load(pack_t *P, const char *dir)
{
	P->dir = strdup(dir);
	char path[1200], line[4096];
	int cn = 0, cc = 0, cnorm = 0;
	snprintf(path, sizeof(path), "%s/index.tsv", dir);
	FILE *f = fopen(path, "r");
	if (f) {
		while (fgets(line, sizeof(line), f)) {
			if (line[0] == '#') continue;
			char *c[4];
			if (tsv_split(line, c, 4) < 4 || !c[3][0]) continue;
			char name[1024];
			snprintf(name, sizeof(name), "%s", c[0]);
			lower(name);
			kv_add(&P->by_name, &P->nname, &cn, name, c[3]);
			if (c[1][0] && c[2][0]) {
				char k[64];
				snprintf(k, sizeof(k), "%.8s:%.20s", c[1], c[2]);
				lower(k);
				kv_add(&P->by_crc, &P->ncrc, &cc, k, c[3]);
			}
			char nb[512];
			title_norm(c[0], nb, sizeof(nb));
			if (nb[0]) kv_add(&P->by_norm, &P->nnorm, &cnorm, nb, c[3]);
		}
		fclose(f);
	}
	if (P->nname > 1) qsort(P->by_name, (size_t)P->nname, sizeof(kv_t), kv_cmp);
	if (P->ncrc > 1) qsort(P->by_crc, (size_t)P->ncrc, sizeof(kv_t), kv_cmp);
	if (P->nnorm > 1) qsort(P->by_norm, (size_t)P->nnorm, sizeof(kv_t), kv_cmp);
	/* the bare title is used only when it means one game: titles with several keys are marked "" */
	for (int i = 0; i + 1 < P->nnorm; i++) {
		int j = i;
		while (j + 1 < P->nnorm && !strcmp(P->by_norm[j + 1].a, P->by_norm[i].a)) j++;
		int same = 1;
		for (int k = i + 1; k <= j; k++) if (strcmp(P->by_norm[k].b, P->by_norm[i].b)) same = 0;
		if (!same) for (int k = i; k <= j; k++) { free(P->by_norm[k].b); P->by_norm[k].b = strdup(""); }
		i = j;
	}
	/* details */
	int ci = 0;
	snprintf(path, sizeof(path), "%s/gameinfo.tsv", dir);
	if ((f = fopen(path, "r"))) {
		while (fgets(line, sizeof(line), f)) {
			if (line[0] == '#') continue;
			char *c[6];
			if (tsv_split(line, c, 6) < 6 || !c[0][0]) continue;
			if (P->ninfo == ci) {
				int nc = ci ? ci * 2 : 1024;
				info_t *ni = realloc(P->info, (size_t)nc * sizeof(*ni));
				if (!ni) break;
				P->info = ni;
				ci = nc;
			}
			info_t *in = &P->info[P->ninfo];
			memset(in, 0, sizeof(*in));
			if (!(in->key = strdup(c[0]))) break;
			P->ninfo++;
			if (c[2][0]) in->f[F_YEAR] = strndup(c[2], 4);
			if (c[3][0]) in->f[F_GENRE] = strdup(c[3]);
			if (c[4][0]) in->f[F_DEV] = strdup(c[4]);
			if (c[5][0]) in->f[F_PLAYERS] = strdup(c[5]);
		}
		fclose(f);
	}
	if (P->ninfo > 1) qsort(P->info, (size_t)P->ninfo, sizeof(info_t), info_cmp);
	/* the description: English first, else any language there is */
	snprintf(path, sizeof(path), "%s/synopsis_en.tsv", dir);
	f = fopen(path, "r");
	if (!f) {
		DIR *dp = opendir(dir);
		struct dirent *de;
		while (dp && !f && (de = readdir(dp)) != NULL)
			if (!strncmp(de->d_name, "synopsis_", 9) && strstr(de->d_name, ".tsv")) {
				snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
				f = fopen(path, "r");
			}
		if (dp) closedir(dp);
	}
	if (f) {
		char *big = malloc(65536);
		while (big && fgets(big, 65536, f)) {
			if (big[0] == '#') continue;
			char *c[2];
			if (tsv_split(big, c, 2) < 2 || !c[1][0]) continue;
			info_t k = { .key = c[0] };
			info_t *in = P->ninfo ? bsearch(&k, P->info, (size_t)P->ninfo, sizeof(info_t), info_cmp) : NULL;
			if (in && !in->f[F_DESC]) in->f[F_DESC] = strdup(c[1]);
		}
		free(big);
		fclose(f);
	}
	plog("local: artwork pack %s (%d names, %d with details)", dir, P->nname, P->ninfo);
}

/* the pack for a games folder name (GAMEBOY, PSX...), from the first drive that has one; NULL if none */
static pack_t *pack_for(const char *folder)
{
	for (int i = 0; i < npacks; i++) if (!strcmp(pack_folder[i], folder)) return packs[i].dir ? &packs[i] : NULL;
	if (npacks == MAX_PACKS) return NULL;
	int me = npacks++;
	snprintf(pack_folder[me], sizeof(pack_folder[me]), "%s", folder);
	memset(&packs[me], 0, sizeof(packs[me]));
	/* the drives the games are on (their docs/ folder beside games/), the SD card last */
	char roots[12][256], dir[600];
	int nr = p_roots(roots, 12);
	for (int i = 0; i <= nr; i++) {
		char base[300];
		if (i < nr) {
			snprintf(base, sizeof(base), "%s", roots[i]);
			char *s = strrchr(base, '/');
			if (!s || strcmp(s, "/games")) continue;
			*s = 0;
		} else snprintf(base, sizeof(base), "%s", p_fat());
		snprintf(dir, sizeof(dir), "%s/docs/%s/Artwork", base, folder);
		if (is_dir(dir)) { pack_load(&packs[me], dir); return &packs[me]; }
	}
	return NULL;
}

/* the key's picture in a pack: 1 and its path, if the file is there */
static int pack_pic(pack_t *P, const char *key, char *out, size_t n)
{
	if (!key || !*key || strchr(key, '/')) return 0;
	snprintf(out, n, "%s/%s.jpg", P->dir, key);
	return is_file(out);
}

/* PACK_FORMAT.md's matching, in order; the key found (into key) and its picture */
static int pack_match(pack_t *P, const sgame_t *g, const char *base, long size, char *key, size_t kn, char *pic, size_t pn)
{
	char low[600];
	const char *k;
	/* 1. the file name is the key */
	if (pack_pic(P, base, pic, pn)) { snprintf(key, kn, "%s", base); return 1; }
	/* arcade: the MAME set name */
	if (g->arcade && g->serial[0] && pack_pic(P, g->serial, pic, pn)) { snprintf(key, kn, "%s", g->serial); return 1; }
	/* 2. the name in index.tsv */
	snprintf(low, sizeof(low), "%s", base);
	lower(low);
	if ((k = kv_get(P->by_name, P->nname, low)) && pack_pic(P, k, pic, pn)) { snprintf(key, kn, "%s", k); return 1; }
	if (g->arcade && g->serial[0]) {
		snprintf(low, sizeof(low), "%s", g->serial);
		lower(low);
		if ((k = kv_get(P->by_name, P->nname, low)) && pack_pic(P, k, pic, pn)) { snprintf(key, kn, "%s", k); return 1; }
	}
	/* 3. a trailing tag that is itself a key */
	size_t bl = strlen(base);
	if (bl > 2 && base[bl - 1] == ')') {
		const char *o = strrchr(base, '(');
		if (o) {
			char tag[200];
			snprintf(tag, sizeof(tag), "%.*s", (int)(base + bl - 1 - o - 1), o + 1);
			if (pack_pic(P, tag, pic, pn)) { snprintf(key, kn, "%s", tag); return 1; }
		}
	}
	/* 4. CRC and size (a renamed cartridge) */
	if (g->crc[0] && size > 0) {
		char ck[64];
		snprintf(ck, sizeof(ck), "%.8s:%ld", g->crc, size);
		lower(ck);
		if ((k = kv_get(P->by_crc, P->ncrc, ck)) && pack_pic(P, k, pic, pn)) { snprintf(key, kn, "%s", k); return 1; }
	}
	/* 5. the bare title, when only one game has it */
	char nb[512];
	title_norm(base, nb, sizeof(nb));
	if (nb[0] && (k = kv_get(P->by_norm, P->nnorm, nb)) && *k && pack_pic(P, k, pic, pn)) { snprintf(key, kn, "%s", k); return 1; }
	/* the official name a source found earlier in this lookup (No-Intro / Redump: the pack's own keys) */
	if (g->official[0] && pack_pic(P, g->official, pic, pn)) { snprintf(key, kn, "%s", g->official); return 1; }
	return 0;
}

static void pack_details(pack_t *P, const char *key, sres_t *r)
{
	if (!P->ninfo) return;
	info_t k = { .key = (char *)key };
	info_t *in = bsearch(&k, P->info, (size_t)P->ninfo, sizeof(info_t), info_cmp);
	if (!in) return;
	for (int i = 0; i < F_COUNT; i++) if (in->f[i]) sres_set(r, i, in->f[i]);
}

/* ---------- the source ---------- */

void local_reset(void)
{
	pthread_mutex_lock(&mu);
	for (int i = 0; i < ndirs; i++) {
		for (int k = 0; k < dirs[i].n; k++) { free(dirs[i].e[k].file); free(dirs[i].e[k].base); free(dirs[i].e[k].norm); free(dirs[i].e[k].tags); }
		free(dirs[i].e);
		free(dirs[i].path);
	}
	ndirs = 0;
	for (int i = 0; i < nlists; i++) {
		for (int k = 0; k < lists[i].n; k++) {
			free(lists[i].e[k].path);
			free(lists[i].e[k].image);
			for (int f = 0; f < F_COUNT; f++) free(lists[i].e[k].f[f]);
		}
		free(lists[i].e);
		free(lists[i].dir);
	}
	nlists = 0;
	for (int i = 0; i < npacks; i++) {
		pack_t *P = &packs[i];
		for (int k = 0; k < P->nname; k++) { free(P->by_name[k].a); free(P->by_name[k].b); }
		for (int k = 0; k < P->ncrc; k++) { free(P->by_crc[k].a); free(P->by_crc[k].b); }
		for (int k = 0; k < P->nnorm; k++) { free(P->by_norm[k].a); free(P->by_norm[k].b); }
		for (int k = 0; k < P->ninfo; k++) { free(P->info[k].key); for (int f = 0; f < F_COUNT; f++) free(P->info[k].f[f]); }
		free(P->by_name); free(P->by_crc); free(P->by_norm); free(P->info); free(P->dir);
	}
	npacks = 0;
	pthread_mutex_unlock(&mu);
}

/* where the game's file is, and its size */
static int game_file(sqlite3 *db, long long id, char *path, size_t n, long *size)
{
	sqlite3_stmt *st;
	int ok = 0;
	if (sqlite3_prepare_v2(db, "SELECT path, size FROM files WHERE game_id=? AND online=1 ORDER BY path LIMIT 1", -1, &st, NULL) != SQLITE_OK) return 0;
	sqlite3_bind_int64(st, 1, id);
	if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_text(st, 0)) {
		snprintf(path, n, "%s", sqlite3_column_text(st, 0));
		*size = (long)sqlite3_column_int64(st, 1);
		ok = 1;
	}
	sqlite3_finalize(st);
	return ok;
}

static void art_file(sres_t *r, const char *path)
{
	char u[1800];
	for (int i = 0; i < r->nart; i++) if (!strcmp(r->art[i] + (strncmp(r->art[i], "file:", 5) ? 0 : 5), path)) return;
	snprintf(u, sizeof(u), "file:%s", path);
	sres_art(r, "local", u);
}

/* the folder above, if it's still inside the system's folder: never games/ itself (other systems' art)
   or above _Arcade. 0: stop here */
static int up_one(char *d)
{
	size_t l = strlen(d);
	if (l >= 8 && !strcmp(d + l - 8, "/_Arcade")) return 0;
	char *s = strrchr(d, '/');
	if (!s || s == d) return 0;
	*s = 0;
	l = strlen(d);
	if (l >= 6 && !strcmp(d + l - 6, "/games")) return 0;
	return 1;
}

static int local_lookup(sqlite3 *db, sgame_t *g, sres_t *r)
{
	char path[1024];
	long size = 0;
	if (!game_file(db, g->id, path, sizeof(path), &size)) return 0;
	const gsys_t *gs = system_by_id(g->sys);
	/* the file's name without its extension, and the folders it's in up to the system folder */
	char base[512], dir[1024];
	const char *slash = strrchr(path, '/');
	snprintf(base, sizeof(base), "%s", slash ? slash + 1 : path);
	char *dot = strrchr(base, '.');
	const char *ext = dot ? dot + 1 : "";
	char extl[16];
	snprintf(extl, sizeof(extl), "%s", ext);
	lower(extl);
	if (dot) *dot = 0;
	snprintf(dir, sizeof(dir), "%.*s", slash ? (int)(slash - path) : 0, path);
	int found = 0;
	pthread_mutex_lock(&mu);

	/* 1. gamelist.xml: this file's entry */
	char d[1024];
	snprintf(d, sizeof(d), "%s", dir);
	for (int up = 0; up < 3; up++) {
		glent_t *e = list_find(list_read(d), path);
		if (e) {
			for (int i = 0; i < F_COUNT; i++) if (e->f[i]) sres_set(r, i, e->f[i]);
			if (e->image && is_file(e->image)) art_file(r, e->image);
			found = 1;
			break;
		}
		if (!up_one(d)) break;
	}

	/* 2. media/ folders, nearest first */
	static const char *const SUB[] = { "media/box2d", "media/covers", "media/boxart", "media/boxFront", "media", "media/box3d", "media/images", NULL };
	snprintf(d, sizeof(d), "%s", dir);
	for (int up = 0; up < 3; up++) {
		for (int i = 0; SUB[i]; i++) {
			char md[1100], pic[1600];
			snprintf(md, sizeof(md), "%s/%s", d, SUB[i]);
			if (dir_find(md, base, pic, sizeof(pic))) { art_file(r, pic); found = 1; }
		}
		char txt[1200];
		snprintf(txt, sizeof(txt), "%s/media/%s.txt", d, base);
		if (is_file(txt)) {
			FILE *f = fopen(txt, "rb");
			if (f) {
				char *desc = calloc(1, 8192);
				size_t got = desc ? fread(desc, 1, 8191, f) : 0;
				fclose(f);
				if (desc) { desc[got] = 0; sres_set(r, F_DESC, desc); found = 1; free(desc); }
			}
		}
		if (!up_one(d)) break;
	}

	/* 3. the artwork packs: each of the system's games folders, and Game Boy's / Color's other one */
	if (gs) {
		char folders[160], *save = NULL;
		snprintf(folders, sizeof(folders), "%s", gs->folders);
		if (!strcmp(extl, "fds") && strstr(gs->folders, "NES")) snprintf(folders, sizeof(folders), "FDS|%s", gs->folders);
		if (!strcmp(gs->folders, "GAMEBOY")) snprintf(folders, sizeof(folders), "GAMEBOY|GBC");
		else if (!strcmp(gs->folders, "GBC")) snprintf(folders, sizeof(folders), "GBC|GAMEBOY");
		if (!strcmp(g->sys, "Arcade")) snprintf(folders, sizeof(folders), "Arcade|_Arcade");
		for (char *fo = strtok_r(folders, "|", &save); fo; fo = strtok_r(NULL, "|", &save)) {
			pack_t *P = pack_for(fo);
			if (!P) continue;
			char key[600], pic[1300];
			if (pack_match(P, g, base, size, key, sizeof(key), pic, sizeof(pic))) {
				pack_details(P, key, r);
				art_file(r, pic);
				found = 1;
				break;
			}
		}
	}
	pthread_mutex_unlock(&mu);
	return found;
}

static int local_ready(void) { return 1; }

const source_t SRC_LOCAL = { "local", "Local files", NULL, local_ready, local_lookup };
