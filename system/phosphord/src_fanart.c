/* phosphord scraper: fan-made box art (the same list Phosphor for Windows and Android use,
 * apps/windows/fanart/fanart.json on GitHub): covers for games no database has, such as ROM hacks and
 * homebrew. Asked before the other sources, by the game's name only: its exact name, then its name
 * with tags, versions and fan-version words out (title_norm). Each cover is fetched from where its
 * maker posted it, or, when that link is gone, from Phosphor's copy in pack.zip ("pack:<file>").
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#include "scraper.h"
#include "scrape_src.h"
#include "cJSON.h"
#include "ident.h"
#include "log.h"
#include "miniz.h"
#include "net.h"
#include "paths.h"
#include <ctype.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define FAN_URL  "https://raw.githubusercontent.com/f3bandit/phosphor_os/main/apps/windows/fanart/fanart.json"
#define PACK_URL "https://raw.githubusercontent.com/f3bandit/phosphor_os/main/apps/windows/fanart/pack.zip"

static pthread_mutex_t fan_lk = PTHREAD_MUTEX_INITIALIZER;
static cJSON *fan_root;          /* the list */
static cJSON *fan_keys;          /* "=sys|exact" and "sys|loose" -> index in games */
static time_t fan_at;

static void exact_key(const char *t, char *out, size_t n)
{
	size_t o = 0;
	for (const unsigned char *p = (const unsigned char *)t; *p && o + 1 < n; p++)
		if (isalnum(*p) || *p > 127) out[o++] = (char)tolower(*p);
	out[o] = 0;
}

static void loose_key(const char *t, char *out, size_t n)
{
	char w[512];
	title_norm(t, w, sizeof(w));
	size_t o = 0;
	for (const char *p = w; *p && o + 1 < n; p++) if (*p != ' ') out[o++] = *p;
	out[o] = 0;
}

static void fan_index(void)
{
	cJSON_Delete(fan_keys);
	fan_keys = cJSON_CreateObject();
	cJSON *clash = cJSON_CreateObject();
	cJSON *games = cJSON_GetObjectItem(fan_root, "games");
	int i = 0;
	for (cJSON *g = games ? games->child : NULL; g; g = g->next, i++) {
		const char *sys = cJSON_GetStringValue(cJSON_GetObjectItem(g, "sys")), *art = cJSON_GetStringValue(cJSON_GetObjectItem(g, "art"));
		if (!sys || !art || !*art) continue;
		cJSON *names = cJSON_GetObjectItem(g, "names");
		for (cJSON *nm = names ? names->child : NULL; nm; nm = nm->next) {
			const char *s = cJSON_GetStringValue(nm);
			if (!s) continue;
			char k[300], key[340];
			exact_key(s, k, sizeof(k));
			if (*k) { snprintf(key, sizeof(key), "=%s|%s", sys, k); cJSON_DeleteItemFromObject(fan_keys, key); cJSON_AddNumberToObject(fan_keys, key, i); }
			loose_key(s, k, sizeof(k));
			if (!*k) continue;
			snprintf(key, sizeof(key), "%s|%s", sys, k);
			cJSON *had = cJSON_GetObjectItem(fan_keys, key);
			if (had && had->valueint != i) cJSON_AddTrueToObject(clash, key);   /* two games: no loose guess */
			cJSON_DeleteItemFromObject(fan_keys, key);
			cJSON_AddNumberToObject(fan_keys, key, i);
		}
	}
	for (cJSON *c = clash->child; c; c = c->next) cJSON_DeleteItemFromObject(fan_keys, c->string);
	cJSON_Delete(clash);
}

static char *read_all(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *b = n > 0 && n < (8L << 20) ? malloc((size_t)n + 1) : NULL;
	if (b && fread(b, 1, (size_t)n, f) == (size_t)n) b[n] = 0; else { free(b); b = NULL; }
	fclose(f);
	return b;
}

/* the list: read again from GitHub after an hour; the copy kept in data/ref when GitHub can't be reached */
static void fan_load(void)
{
	time_t now = time(NULL);
	if (fan_root && now - fan_at < 3600) return;
	fan_at = now;
	char path[600], dir[560];
	snprintf(dir, sizeof(dir), "%s/ref", p_data());
	mkdir(dir, 0755);
	snprintf(path, sizeof(path), "%s/fanart.json", dir);
	int rc = net_get_file(FAN_URL, path);
	if (rc && fan_root) return;                  /* keep what we have */
	char *t = read_all(path);
	cJSON *j = t ? cJSON_Parse(t) : NULL;
	free(t);
	if (!cJSON_IsArray(cJSON_GetObjectItem(j, "games"))) { cJSON_Delete(j); return; }
	cJSON_Delete(fan_root);
	fan_root = j;
	fan_index();
}

static int fan_lookup(sqlite3 *db, sgame_t *g, sres_t *r)
{
	(void)db;
	pthread_mutex_lock(&fan_lk);
	fan_load();
	int found = 0;
	if (fan_keys) {
		char k[300], key[340];
		exact_key(g->title, k, sizeof(k));
		snprintf(key, sizeof(key), "=%s|%s", g->sys, k);
		cJSON *hit = *k ? cJSON_GetObjectItem(fan_keys, key) : NULL;
		if (!hit) {
			loose_key(g->title, k, sizeof(k));
			snprintf(key, sizeof(key), "%s|%s", g->sys, k);
			hit = *k ? cJSON_GetObjectItem(fan_keys, key) : NULL;
		}
		cJSON *e = hit ? cJSON_GetArrayItem(cJSON_GetObjectItem(fan_root, "games"), hit->valueint) : NULL;
		if (e) {
			const char *desc = cJSON_GetStringValue(cJSON_GetObjectItem(e, "description")), *by = cJSON_GetStringValue(cJSON_GetObjectItem(e, "hack_by"));
			char d[2048];
			snprintf(d, sizeof(d), "%s", desc ? desc : "");
			if (by && *by && !strstr(d, by)) { size_t l = strlen(d); snprintf(d + l, sizeof(d) - l, "%sHack by %s.", l ? " " : "", by); }
			sres_set(r, F_DESC, d);
			sres_set(r, F_DEV, cJSON_GetStringValue(cJSON_GetObjectItem(e, "developer")));
			sres_set(r, F_YEAR, cJSON_GetStringValue(cJSON_GetObjectItem(e, "year")));
			sres_set(r, F_GENRE, cJSON_GetStringValue(cJSON_GetObjectItem(e, "genre")));
			sres_art(r, "fan art", cJSON_GetStringValue(cJSON_GetObjectItem(e, "art")));
			const char *pk = cJSON_GetStringValue(cJSON_GetObjectItem(e, "pack"));
			if (pk && *pk) { char u[300]; snprintf(u, sizeof(u), "pack:%s", pk); sres_art(r, "fan art pack", u); }
			found = 1;
		}
	}
	pthread_mutex_unlock(&fan_lk);
	return found;
}

/* ---------- the backup pack: downloaded once, only when a cover's link is gone ---------- */

static uint32_t le32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

/* one file out of a zip into dest: 0 done, 1 not in it, -1 a damaged zip */
static int zip_extract(const char *zip, const char *name, const char *dest)
{
	FILE *f = fopen(zip, "rb");
	if (!f) return -1;
	unsigned char tail[66000];
	fseek(f, 0, SEEK_END);
	long fsz = ftell(f), tl = fsz < (long)sizeof(tail) ? fsz : (long)sizeof(tail);
	fseek(f, fsz - tl, SEEK_SET);
	if (tl < 22 || fread(tail, 1, (size_t)tl, f) != (size_t)tl) { fclose(f); return -1; }
	long eocd = -1;
	for (long i = tl - 22; i >= 0; i--) if (tail[i] == 0x50 && tail[i + 1] == 0x4b && tail[i + 2] == 5 && tail[i + 3] == 6) { eocd = i; break; }
	if (eocd < 0) { fclose(f); return -1; }
	uint32_t cd_size = le32(tail + eocd + 12), cd_off = le32(tail + eocd + 16);
	if (cd_size < 46 || cd_size > (4u << 20) || (long)cd_off + (long)cd_size > fsz) { fclose(f); return -1; }
	unsigned char *cd = malloc(cd_size);
	fseek(f, (long)cd_off, SEEK_SET);
	if (!cd || fread(cd, 1, cd_size, f) != cd_size) { free(cd); fclose(f); return -1; }
	long local = -1;
	uint32_t csize = 0, usize = 0;
	int method = -1;
	size_t want = strlen(name);
	for (uint32_t p = 0; p + 46 <= cd_size;) {
		uint16_t nl = cd[p + 28] | cd[p + 29] << 8, el = cd[p + 30] | cd[p + 31] << 8, cl = cd[p + 32] | cd[p + 33] << 8;
		if ((uint64_t)p + 46 + nl + el + cl > cd_size) break;
		if (nl == want && !memcmp(cd + p + 46, name, want)) {
			method = cd[p + 10] | cd[p + 11] << 8;
			csize = le32(cd + p + 20); usize = le32(cd + p + 24); local = le32(cd + p + 42);
			break;
		}
		p += 46 + nl + el + cl;
	}
	free(cd);
	if (local < 0) { fclose(f); return 1; }
	if ((method != 0 && method != 8) || usize > (32u << 20) || csize > (32u << 20)) { fclose(f); return -1; }
	unsigned char lh[30];
	fseek(f, local, SEEK_SET);
	if (fread(lh, 1, 30, f) != 30) { fclose(f); return -1; }
	fseek(f, local + 30 + (lh[26] | lh[27] << 8) + (lh[28] | lh[29] << 8), SEEK_SET);
	unsigned char *in = malloc(csize ? csize : 1), *out = malloc(usize ? usize : 1);
	int ok = in && out && fread(in, 1, csize, f) == csize;
	fclose(f);
	if (ok && method == 8) {
		mz_stream z;
		memset(&z, 0, sizeof(z));
		ok = mz_inflateInit2(&z, -MZ_DEFAULT_WINDOW_BITS) == MZ_OK;
		if (ok) {
			z.next_in = in; z.avail_in = csize; z.next_out = out; z.avail_out = usize;
			ok = mz_inflate(&z, MZ_FINISH) == MZ_STREAM_END && z.total_out == usize;
			mz_inflateEnd(&z);
		}
	} else if (ok) memcpy(out, in, usize);
	if (ok) {
		FILE *o = fopen(dest, "wb");
		ok = o && fwrite(out, 1, usize, o) == usize;
		if (o && fclose(o)) ok = 0;
	}
	free(in); free(out);
	return ok ? 0 : -1;
}

int fanart_pack_get(const char *name, const char *dest)
{
	char zip[600];
	snprintf(zip, sizeof(zip), "%s/ref/fanart-pack.zip", p_data());
	pthread_mutex_lock(&fan_lk);
	int rc = access(zip, F_OK) == 0 ? zip_extract(zip, name, dest) : -1;
	if (rc < 0) {                                 /* not here yet (or damaged): fetch it */
		int n = net_get_file(PACK_URL, zip);
		rc = n == 0 ? zip_extract(zip, name, dest) : n < 0 ? -2 : 1;
		if (n == 0 && rc < 0) plog("fanart: the pack doesn't read");
	}
	pthread_mutex_unlock(&fan_lk);
	return rc == 0 ? 0 : rc == -2 ? -1 : 1;         /* as net_get_file: -1 network trouble */
}

static int fan_ready(void) { return 1; }

const source_t SRC_FANART = { "fanart", "Phosphor's fan art list", NULL, fan_ready, fan_lookup };
