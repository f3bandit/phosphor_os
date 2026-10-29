/* phosphord: wallpapers (Settings > Wallpapers). Update All's wallpaper collections all land in
 * /media/fat/wallpapers; MiSTer's downloader keeps which file came from which collection
 * (Scripts/.config/downloader/downloader.json: dbs.<id>.files), so each creator gets a category. The
 * person's own go in /media/fat/wallpapers_user. A picture's aspect comes from its name (4X3, 16X9:
 * Ranny Snice's) or its size.
 *   wallpapers [category] [aspect] [current]   {categories:[{id,name,count}], items:[{file,name,aspect}], osd,
 *                                    current_category: the one the picture in use is in}
 *   wallpaper_thumb file             a small JPEG of it: {thumb}, or {pending} while the worker makes it
 *   wallpaper_prefetch files         the worker makes these next (the page on screen, then the next one)
 *   wallpaper_screen file w h        the picture made to cover the screen, darkened: {screen}
 *   wallpaper_osd file|""            behind MiSTer's own menu (Phosphor's Main reads
 *                                    phosphor/config/osd_wallpaper); "" puts MiSTer's usual back
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#include "wallpaper.h"
#include "art.h"
#include "log.h"
#include "paths.h"
#include <dirent.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

/* Update All's wallpaper collections (Update_All_MiSTer databases.py) */
static const struct { const char *db, *id, *name; } COLL[] = {
	{ "Ranny-Snice/Ranny-Snice-Wallpapers", "rannysnice", "Ranny Snice" },
	{ "anime0t4ku_wallpapers", "anime0t4ku", "Anime0t4ku" },
	{ "pcn_challenge_wallpapers", "pcn", "PCN Challenge" },
	{ "RGarciaLago/Wallpaper_Collection", "rgarcialago", "RGarciaLago" },
};
#define NCOLL ((int)(sizeof(COLL) / sizeof(COLL[0])))
#define WP_DIM 150                              /* how far the screen version is darkened (of 255) */

static int is_image(const char *n)
{
	const char *d = strrchr(n, '.');
	return d && (!strcasecmp(d, ".jpg") || !strcasecmp(d, ".jpeg") || !strcasecmp(d, ".png"));
}

/* which collection each file in wallpapers/ came from: kept in data/wallpapers.json, worked out again
   when the downloader's file changes (it's a few MB: read once) */
static cJSON *owners(void)
{
	char dj[600], cache[600];
	snprintf(dj, sizeof(dj), "%s/Scripts/.config/downloader/downloader.json", p_fat());
	snprintf(cache, sizeof(cache), "%s/wallpapers.json", p_data());
	struct stat a, b;
	int have = stat(dj, &a) == 0;
	if (have && stat(cache, &b) == 0 && b.st_mtime >= a.st_mtime) {
		FILE *f = fopen(cache, "r");
		if (f) {
			fseek(f, 0, SEEK_END);
			long n = ftell(f);
			fseek(f, 0, SEEK_SET);
			char *t = n > 0 && n < (8 << 20) ? malloc((size_t)n + 1) : NULL;
			cJSON *o = NULL;
			if (t && fread(t, 1, (size_t)n, f) == (size_t)n) { t[n] = 0; o = cJSON_Parse(t); }
			free(t);
			fclose(f);
			if (o) return o;
		}
	}
	cJSON *map = cJSON_CreateObject();          /* "wallpapers/<file>" -> collection id */
	if (have) {
		FILE *f = fopen(dj, "r");
		char *t = NULL;
		long n = 0;
		if (f) {
			fseek(f, 0, SEEK_END);
			n = ftell(f);
			fseek(f, 0, SEEK_SET);
			t = n > 0 && n < (64 << 20) ? malloc((size_t)n + 1) : NULL;
			if (t && fread(t, 1, (size_t)n, f) != (size_t)n) { free(t); t = NULL; }
			if (t) t[n] = 0;
			fclose(f);
		}
		cJSON *d = t ? cJSON_Parse(t) : NULL;
		free(t);
		cJSON *dbs = cJSON_GetObjectItem(d, "dbs");
		for (int i = 0; i < NCOLL; i++) {
			cJSON *db = cJSON_GetObjectItem(dbs, COLL[i].db), *fl = cJSON_GetObjectItem(db, "files"), *e;
			cJSON_ArrayForEach(e, fl)
				if (e->string && !strncasecmp(e->string, "wallpapers/", 11) && is_image(e->string))
					cJSON_AddStringToObject(map, e->string + 11, COLL[i].id);
		}
		cJSON_Delete(d);
		char *s = cJSON_PrintUnformatted(map);
		char tmp[620];
		snprintf(tmp, sizeof(tmp), "%s.tmp", cache);
		FILE *w = s ? fopen(tmp, "w") : NULL;
		if (w) { fputs(s, w); fclose(w); rename(tmp, cache); }
		free(s);
		plog("wallpapers: %d files from Update All's collections", cJSON_GetArraySize(map));
	}
	return map;
}

static const char *aspect_of(const char *path, const char *name)
{
	if (strcasestr(name, "4x3") || strcasestr(name, "4_3")) return "4:3";
	if (strcasestr(name, "16x9") || strcasestr(name, "16_9")) return "16:9";
	int w, h;
	if (art_size(path, &w, &h) == 0 && h > 0) return w * 100 / h >= 150 ? "16:9" : "4:3";
	return "16:9";
}

/* a readable name: the file's, without the extension, the 4X3 / 16X9 tag and "MiSTer " */
static void wp_name(const char *fn, char *out, size_t n)
{
	snprintf(out, n, "%s", fn);
	char *d = strrchr(out, '.');
	if (d) *d = 0;
	const char *tags[] = { "MiSTer 16X9 ", "MiSTer 4X3 ", "MiSTer ", "16X9 ", "4X3 ", "16x9_", "4x3_" };
	for (size_t i = 0; i < sizeof(tags) / sizeof(tags[0]); i++) {
		size_t l = strlen(tags[i]);
		if (!strncasecmp(out, tags[i], l)) { memmove(out, out + l, strlen(out + l) + 1); break; }
	}
	for (char *c = out; *c; c++) if (*c == '_') *c = ' ';
	/* "MiSTer - Anime0t4ku - Metal Gear": the leading dash, then the creator's name (it's the tab) */
	for (int again = 1; again;) {
		again = 0;
		size_t l = strspn(out, " -");
		if (l) { memmove(out, out + l, strlen(out + l) + 1); again = 1; }
		for (int i = 0; i < NCOLL; i++) {
			size_t cl = strlen(COLL[i].name);
			if (!strncasecmp(out, COLL[i].name, cl) && (out[cl] == ' ' || out[cl] == '-')) {
				memmove(out, out + cl, strlen(out + cl) + 1);
				again = 1;
			}
		}
	}
	if (!out[0]) snprintf(out, n, "%s", fn);
}

cJSON *wallpaper_list(const char *cat, const char *aspect, const char *current)
{
	const char *cur_cat = NULL;
	cJSON *o = cJSON_CreateObject(), *cats = cJSON_AddArrayToObject(o, "categories"), *items = cJSON_AddArrayToObject(o, "items");
	cJSON *own = owners();
	int count[NCOLL + 2] = { 0 };               /* [0] the person's, [1..] collections, [NCOLL+1] other */
	const char *dirs[2] = { "wallpapers_user", "wallpapers" };
	{
		char u[600];                                /* the person's own folder, so it's there to put pictures in */
		snprintf(u, sizeof(u), "%s/wallpapers_user", p_fat());
		mkdirs(u);
	}
	for (int di = 0; di < 2; di++) {
		char d[600];
		snprintf(d, sizeof(d), "%s/%s", p_fat(), dirs[di]);
		DIR *dd = opendir(d);
		struct dirent *e;
		while (dd && (e = readdir(dd))) {
			if (e->d_name[0] == '.' || !is_image(e->d_name)) continue;
			int ci = 0;
			const char *cid = "user";
			if (di == 1) {
				const char *c = cJSON_GetStringValue(cJSON_GetObjectItem(own, e->d_name));
				ci = NCOLL + 1;
				cid = "other";
				for (int i = 0; c && i < NCOLL; i++) if (!strcmp(c, COLL[i].id)) { ci = i + 1; cid = COLL[i].id; }
			}
			char path[900], nm[300];
			snprintf(path, sizeof(path), "%s/%s", d, e->d_name);
			if (current && !strcmp(current, path)) cur_cat = cid;   /* (a static string: "user" or COLL's) */
			const char *asp = aspect_of(path, e->d_name);
			if (aspect && *aspect && strcmp(aspect, "all") && strcmp(asp, aspect)) continue;
			count[ci]++;
			if (!cat || strcmp(cat, cid)) continue;
			wp_name(e->d_name, nm, sizeof(nm));
			cJSON *x = cJSON_CreateObject();
			cJSON_AddStringToObject(x, "file", path);
			cJSON_AddStringToObject(x, "name", nm);
			cJSON_AddStringToObject(x, "aspect", asp);
			cJSON_AddItemToArray(items, x);
		}
		if (dd) closedir(dd);
	}
	cJSON_Delete(own);
	/* yours (always, so there's somewhere to start), then the ones with something in them (in this aspect) */
	for (int i = 0; i < NCOLL + 2; i++) {
		if (!count[i] && i) continue;
		cJSON *c = cJSON_CreateObject();
		cJSON_AddStringToObject(c, "id", i == 0 ? "user" : i == NCOLL + 1 ? "other" : COLL[i - 1].id);
		cJSON_AddStringToObject(c, "name", i == 0 ? "Your wallpapers" : i == NCOLL + 1 ? "Other wallpapers" : COLL[i - 1].name);
		cJSON_AddNumberToObject(c, "count", count[i]);
		cJSON_AddItemToArray(cats, c);
	}
	int n = cJSON_GetArraySize(items);           /* by name */
	for (int i = 1; i < n; i++)
		for (int j = i; j > 0; j--) {
			cJSON *p = cJSON_GetArrayItem(items, j - 1), *q = cJSON_GetArrayItem(items, j);
			if (strcasecmp(cJSON_GetStringValue(cJSON_GetObjectItem(p, "name")), cJSON_GetStringValue(cJSON_GetObjectItem(q, "name"))) <= 0) break;
			cJSON_InsertItemInArray(items, j - 1, cJSON_DetachItemFromArray(items, j));
		}
	if (cur_cat) cJSON_AddStringToObject(o, "current_category", cur_cat);
	char op[600], osd[900] = "";
	snprintf(op, sizeof(op), "%s/phosphor/config/osd_wallpaper", p_fat());
	FILE *f = fopen(op, "r");
	if (f) { if (fgets(osd, sizeof(osd), f)) osd[strcspn(osd, "\r\n")] = 0; fclose(f); }
	if (osd[0]) {
		char full[1000];
		snprintf(full, sizeof(full), "%s/%s", p_fat(), osd);
		cJSON_AddStringToObject(o, "osd", full);
	}
	return o;
}

/* only pictures in the two wallpaper folders */
static int ok_file(const char *file, char *real)
{
	char a[600], b[600];
	snprintf(a, sizeof(a), "%s/wallpapers/", p_fat());
	snprintf(b, sizeof(b), "%s/wallpapers_user/", p_fat());
	return file && realpath(file, real) && (!strncmp(real, a, strlen(a)) || !strncmp(real, b, strlen(b))) && is_image(real);
}

static unsigned long long fnv(const char *s)
{
	unsigned long long h = 1469598103934665603ULL;
	for (; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ULL; }
	return h;
}

/* where a picture's thumbnail goes (named by its path, size and date, so a changed picture gets a new one) */
static int thumb_path(const char *real, char *out, size_t n)
{
	char dir[600];
	snprintf(dir, sizeof(dir), "%s/wall_thumbs", p_data());
	mkdirs(dir);
	struct stat s;
	if (stat(real, &s)) return -1;
	char key[PATH_MAX + 40];
	snprintf(key, sizeof(key), "%s|%lld|%lld", real, (long long)s.st_size, (long long)s.st_mtime);
	snprintf(out, n, "%s/%016llx.jpg", dir, fnv(key));
	return 0;
}

/* the thumbnail workers: a big wallpaper takes a second or more to shrink on the SS1 (a JPEG is decoded
   at 1/8 size, a PNG in full), so the picker never waits for one. Two threads, at low priority, working through a short queue the picker sets (the page
   on screen first); it stops when the queue is empty */
#define WQ_MAX 32
static pthread_mutex_t wq_lk = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wq_cv = PTHREAD_COND_INITIALIZER;
#define WQ_THREADS 2                            /* the SS1's two cores: a big PNG still takes a few seconds */
static char wq[WQ_MAX][PATH_MAX];
static int wq_n, wq_started;
static char wq_busy[WQ_THREADS][PATH_MAX];      /* the ones being made */

static void *wq_thread(void *arg)
{
	int me = (int)(intptr_t)arg;
	setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), 15);
	for (;;) {
		char src[PATH_MAX], out[700];
		pthread_mutex_lock(&wq_lk);
		while (!wq_n) pthread_cond_wait(&wq_cv, &wq_lk);
		snprintf(src, sizeof(src), "%s", wq[0]);
		memmove(wq[0], wq[1], sizeof(wq[0]) * (size_t)(--wq_n));
		snprintf(wq_busy[me], sizeof(wq_busy[me]), "%s", src);
		pthread_mutex_unlock(&wq_lk);
		if (thumb_path(src, out, sizeof(out)) == 0 && !file_exists(out)) {
			char tmp[720];
			snprintf(tmp, sizeof(tmp), "%s.part.jpg", out);
			if (art_thumb(src, NULL, 0, tmp, 320) == 0) rename(tmp, out);
			else { unlink(tmp); plog("wallpapers: couldn't read %s", src); }
		}
		pthread_mutex_lock(&wq_lk);
		wq_busy[me][0] = 0;
		pthread_mutex_unlock(&wq_lk);
	}
	return NULL;
}

/* adds it to the queue (first: in front); the lock is held */
static void wq_add(const char *real, int first)
{
	for (int t = 0; t < WQ_THREADS; t++) if (!strcmp(wq_busy[t], real)) return;
	for (int i = 0; i < wq_n; i++)
		if (!strcmp(wq[i], real)) {
			if (!first) return;
			memmove(wq[1], wq[0], sizeof(wq[0]) * (size_t)i);
			snprintf(wq[0], sizeof(wq[0]), "%s", real);
			return;
		}
	if (wq_n == WQ_MAX) wq_n--;                  /* full: the last one waits for the next ask */
	if (first) { memmove(wq[1], wq[0], sizeof(wq[0]) * (size_t)wq_n); snprintf(wq[0], sizeof(wq[0]), "%s", real); }
	else snprintf(wq[wq_n], sizeof(wq[0]), "%s", real);
	wq_n++;
	for (; wq_started < WQ_THREADS; wq_started++) {
		pthread_t t;
		if (pthread_create(&t, NULL, wq_thread, (void *)(intptr_t)wq_started) != 0) break;
		pthread_detach(t);
	}
	pthread_cond_signal(&wq_cv);
}

int wallpaper_thumb(const char *file, char *out, size_t n)
{
	char real[PATH_MAX];
	if (!ok_file(file, real) || thumb_path(real, out, n)) return -1;
	if (file_exists(out)) return 0;
	pthread_mutex_lock(&wq_lk);
	wq_add(real, 1);
	pthread_mutex_unlock(&wq_lk);
	return 1;
}

void wallpaper_prefetch(cJSON *files)
{
	pthread_mutex_lock(&wq_lk);
	wq_n = 0;                                   /* only what the picker shows now */
	cJSON *f;
	cJSON_ArrayForEach(f, files) {
		char real[PATH_MAX], out[700];
		const char *s = cJSON_GetStringValue(f);
		if (wq_n < WQ_MAX && ok_file(s, real) && thumb_path(real, out, sizeof(out)) == 0 && !file_exists(out)) wq_add(real, 0);
	}
	pthread_mutex_unlock(&wq_lk);
}

int wallpaper_screen(const char *file, int w, int h, char *out, size_t n)
{
	char real[PATH_MAX], dir[600];
	if (!ok_file(file, real) || w < 320 || h < 200 || w > 3840 || h > 2160) return -1;
	snprintf(dir, sizeof(dir), "%s/wall_thumbs", p_data());
	mkdirs(dir);
	struct stat s;
	if (stat(real, &s)) return -1;
	char key[PATH_MAX + 60];
	/* darkened most of the way toward the shell's background, so the menus stay readable over a busy picture */
	snprintf(key, sizeof(key), "%s|%lld|%lld|%dx%d|screen|d%d", real, (long long)s.st_size, (long long)s.st_mtime, w, h, WP_DIM);
	snprintf(out, n, "%s/%016llx-screen.jpg", dir, fnv(key));
	if (file_exists(out)) return 0;
	return art_cover(real, out, w, h, WP_DIM);
}

int wallpaper_osd(const char *file)
{
	char op[600], real[PATH_MAX];
	snprintf(op, sizeof(op), "%s/phosphor/config/osd_wallpaper", p_fat());
	if (!file || !*file) { unlink(op); return 0; }
	if (!ok_file(file, real)) return -1;
	size_t fl = strlen(p_fat());
	const char *rel = !strncmp(real, p_fat(), fl) && real[fl] == '/' ? real + fl + 1 : NULL;
	if (!rel) return -1;
	char tmp[620];
	snprintf(tmp, sizeof(tmp), "%s.tmp", op);
	FILE *f = fopen(tmp, "w");
	if (!f) return -1;
	fprintf(f, "%s\n", rel);
	fclose(f);
	return rename(tmp, op);
}
