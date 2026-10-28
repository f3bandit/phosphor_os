/* phosphord: wallpapers (Settings > Wallpapers). Update All's wallpaper collections all land in
 * /media/fat/wallpapers; MiSTer's downloader keeps which file came from which collection
 * (Scripts/.config/downloader/downloader.json: dbs.<id>.files), so each creator gets a category. The
 * person's own go in /media/fat/wallpapers_user. A picture's aspect comes from its name (4X3, 16X9:
 * Ranny Snice's) or its size.
 *   wallpapers [category] [aspect]   {categories:[{id,name,count}], items:[{file,name,aspect}], osd}
 *   wallpaper_thumb file             a small JPEG of it (made once): {thumb}
 *   wallpaper_screen file w h        the picture made to cover the screen, darkened: {screen}
 *   wallpaper_osd file|""            behind MiSTer's own menu (Phosphor's Main reads
 *                                    phosphor/config/osd_wallpaper); "" puts MiSTer's usual back
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#include "wallpaper.h"
#include "art.h"
#include "log.h"
#include "paths.h"
#include <dirent.h>
#include <limits.h>
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
}

cJSON *wallpaper_list(const char *cat, const char *aspect)
{
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

int wallpaper_thumb(const char *file, char *out, size_t n)
{
	char real[PATH_MAX], dir[600];
	if (!ok_file(file, real)) return -1;
	snprintf(dir, sizeof(dir), "%s/wall_thumbs", p_data());
	mkdirs(dir);
	struct stat s;
	if (stat(real, &s)) return -1;
	char key[PATH_MAX + 40];
	snprintf(key, sizeof(key), "%s|%lld|%lld", real, (long long)s.st_size, (long long)s.st_mtime);
	snprintf(out, n, "%s/%016llx.jpg", dir, fnv(key));
	if (file_exists(out)) return 0;
	return art_thumb(real, NULL, 0, out, 320);
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
	snprintf(key, sizeof(key), "%s|%lld|%lld|%dx%d|screen", real, (long long)s.st_size, (long long)s.st_mtime, w, h);
	snprintf(out, n, "%s/%016llx-screen.jpg", dir, fnv(key));
	if (file_exists(out)) return 0;
	return art_cover(real, out, w, h, 110);
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
