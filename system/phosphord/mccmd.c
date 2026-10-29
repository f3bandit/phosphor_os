/* phosphord: the memory card library's saves, for the TV and the app (the same as SS1 Tool's
 * Files > Memory cards): the cards and their versions, what's on a card, and changes that each make a
 * new version (nothing is ever overwritten): copy / move ticked saves to another card, remove saves,
 * a new blank card, rename, delete a version or a card; one save out (.mcs) or in, a whole version out
 * (.mcd). Writing to a real card stays the memcard "restore" job (a fresh backup first, every time).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 * The memory card manager (first in SS1 Tool): f3bandit's idea and design. */
#include "mccmd.h"
#include "mcformat.h"
#include "mclib.h"
#include "log.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *b64(const unsigned char *d, size_t n)
{
	char *o = malloc((n + 2) / 3 * 4 + 1), *p = o;
	if (!o) return NULL;
	for (size_t i = 0; i < n; i += 3) {
		unsigned v = d[i] << 16 | (i + 1 < n ? d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
		*p++ = B64[v >> 18 & 63];
		*p++ = B64[v >> 12 & 63];
		*p++ = i + 1 < n ? B64[v >> 6 & 63] : '=';
		*p++ = i + 2 < n ? B64[v & 63] : '=';
	}
	*p = 0;
	return o;
}

static unsigned char *unb64(const char *s, size_t *n)
{
	size_t l = strlen(s);
	unsigned char *o = malloc(l / 4 * 3 + 3);
	if (!o) return NULL;
	unsigned v = 0;
	int bits = 0;
	*n = 0;
	for (; *s; s++) {
		const char *k = strchr(B64, *s);
		if (*s == '=' || !k) continue;
		v = v << 6 | (unsigned)(k - B64);
		bits += 6;
		if (bits >= 8) { bits -= 8; o[(*n)++] = (unsigned char)(v >> bits & 255); }
	}
	return o;
}

static void free_versions(char **v, int n) { for (int i = 0; i < n; i++) free(v[i]); free(v); }

static int good_name(const char *s) { return s && *s && !strchr(s, '/') && !strstr(s, "..") && s[0] != '.'; }

static cJSON *save_json(const unsigned char *img, const mc_save_t *s, int icon)
{
	cJSON *o = cJSON_CreateObject();
	cJSON_AddNumberToObject(o, "slot", s->slot);
	cJSON *b = cJSON_AddArrayToObject(o, "blocks");
	for (int i = 0; i < s->nblocks; i++) cJSON_AddItemToArray(b, cJSON_CreateNumber(s->blocks[i]));
	cJSON_AddStringToObject(o, "name", s->name);
	cJSON_AddStringToObject(o, "product", s->product);
	cJSON_AddStringToObject(o, "region", s->region);
	cJSON_AddStringToObject(o, "title", s->title);
	if (s->broken) cJSON_AddBoolToObject(o, "broken", 1);
	if (icon) {                                /* 16x16 RGBA, base64 (the TV draws it; the app makes a PNG) */
		unsigned char px[16 * 16 * 4];
		mc_icon(img, s->slot, px);
		char *e = b64(px, sizeof(px));
		if (e) { cJSON_AddStringToObject(o, "icon", e); free(e); }
	}
	return o;
}

/* the library: every card with its saves, free blocks, versions */
static cJSON *library(void)
{
	cJSON *arr = cJSON_CreateArray();
	DIR *d = opendir(mclib_dir());
	if (!d) return arr;
	unsigned char *img = malloc(MC_SIZE);
	struct dirent *e;
	while (img && (e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		int nv = 0;
		char **v = mclib_versions(e->d_name, &nv);
		if (!nv) { free_versions(v, nv); continue; }
		cJSON *c = cJSON_CreateObject();
		cJSON_AddStringToObject(c, "name", e->d_name);
		cJSON_AddBoolToObject(c, "real", mclib_real(e->d_name));
		cJSON *va = cJSON_AddArrayToObject(c, "versions");
		for (int i = 0; i < nv; i++) {
			char t[256];
			snprintf(t, sizeof(t), "%.*s", (int)strlen(v[i]) - 4, v[i]);
			cJSON_AddItemToArray(va, cJSON_CreateString(t));
		}
		cJSON_AddStringToObject(c, "latest", cJSON_GetArrayItem(va, nv - 1)->valuestring);
		mc_card_t mc;
		if (!mclib_load(e->d_name, NULL, img, NULL, 0)) {
			mc_parse(img, &mc);
			cJSON_AddNumberToObject(c, "saves", mc.nsaves);
			cJSON_AddNumberToObject(c, "free", mc.formatted ? mc.free : 0);
			cJSON_AddBoolToObject(c, "formatted", mc.formatted);
		}
		cJSON_AddItemToArray(arr, c);
		free_versions(v, nv);
	}
	free(img);
	closedir(d);
	return arr;
}

static int store(const char *card, const unsigned char *img, const char **err)
{
	char rel[400];
	int same;
	if (mclib_store(card, img, rel, sizeof(rel), &same)) { *err = "couldn't save the card"; return -1; }
	return 0;
}

static int rm_rf_card(const char *card)
{
	char dir[800], p[1200];
	snprintf(dir, sizeof(dir), "%s/%s", mclib_dir(), card);
	DIR *d = opendir(dir);
	if (!d) return -1;
	struct dirent *e;
	while ((e = readdir(d))) {
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
		snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
		unlink(p);
	}
	closedir(d);
	return rmdir(dir);
}

cJSON *mc_command(cJSON *req, const char **err)
{
	const char *d = cJSON_GetStringValue(cJSON_GetObjectItem(req, "do"));
	const char *name = cJSON_GetStringValue(cJSON_GetObjectItem(req, "name"));
	const char *ver = cJSON_GetStringValue(cJSON_GetObjectItem(req, "version"));
	static unsigned char img[MC_SIZE], dst[MC_SIZE];
	cJSON *o = cJSON_CreateObject();
	if (!strcmp(d, "library")) {
		cJSON_AddItemToObject(o, "library", library());
		return o;
	}
	if (!strcmp(d, "card")) {
		char got[256];
		if (!good_name(name) || mclib_load(name, ver, img, got, sizeof(got))) { *err = "no such card"; cJSON_Delete(o); return NULL; }
		mc_card_t c;
		mc_parse(img, &c);
		cJSON *cj = cJSON_AddObjectToObject(o, "card");
		cJSON_AddBoolToObject(cj, "formatted", c.formatted);
		cJSON_AddNumberToObject(cj, "free", c.free);
		cJSON *sa = cJSON_AddArrayToObject(cj, "saves");
		for (int i = 0; i < c.nsaves; i++) cJSON_AddItemToArray(sa, save_json(img, &c.saves[i], 1));
		int nv = 0;
		char **v = mclib_versions(name, &nv);
		cJSON *va = cJSON_AddArrayToObject(o, "versions");
		for (int i = 0; i < nv; i++) {
			char t[256];
			snprintf(t, sizeof(t), "%.*s", (int)strlen(v[i]) - 4, v[i]);
			cJSON_AddItemToArray(va, cJSON_CreateString(t));
		}
		free_versions(v, nv);
		got[strlen(got) - 4] = 0;
		cJSON_AddStringToObject(o, "version", got);
		return o;
	}
	if (!strcmp(d, "export")) {                /* {name, version, slot?}: .mcs (a save) or .mcd (the version) */
		if (!good_name(name) || mclib_load(name, ver, img, NULL, 0)) { *err = "no such card"; cJSON_Delete(o); return NULL; }
		cJSON *sl = cJSON_GetObjectItem(req, "slot");
		char *e;
		if (cJSON_IsNumber(sl)) {
			unsigned char *x = malloc(128 + 15 * MC_BLOCK);
			size_t n;
			if (!x || mc_export(img, sl->valueint, x, &n, err)) { free(x); cJSON_Delete(o); return NULL; }
			mc_card_t c;
			mc_parse(img, &c);
			for (int i = 0; i < c.nsaves; i++)
				if (c.saves[i].slot == sl->valueint) {
					char fn[160];
					snprintf(fn, sizeof(fn), "%s (%s).mcs", c.saves[i].title, c.saves[i].product);
					for (char *p = fn; *p; p++) if (strchr("\\/:*?\"<>|", *p)) *p = '_';
					cJSON_AddStringToObject(o, "file", fn);
				}
			e = b64(x, n);
			free(x);
		} else {
			char fn[300];
			snprintf(fn, sizeof(fn), "%s.mcd", name);
			cJSON_AddStringToObject(o, "file", fn);
			e = b64(img, MC_SIZE);
		}
		if (!e) { *err = "out of memory"; cJSON_Delete(o); return NULL; }
		cJSON_AddStringToObject(o, "data", e);
		free(e);
		return o;
	}
	if (strcmp(d, "edit")) { *err = "do: library, card, export or edit"; cJSON_Delete(o); return NULL; }
	const char *op = cJSON_GetStringValue(cJSON_GetObjectItem(req, "op"));
	const char *nn = cJSON_GetStringValue(cJSON_GetObjectItem(req, "new_name"));
	const char *to = cJSON_GetStringValue(cJSON_GetObjectItem(req, "to"));
	cJSON *slots = cJSON_GetObjectItem(req, "slots");
	char safe[200];
	if (!op) { *err = "op missing"; cJSON_Delete(o); return NULL; }
	if (!strcmp(op, "new") || !strcmp(op, "rename")) {
		if (!nn || mclib_safe_name(nn, safe, sizeof(safe))) { *err = "give the card a name"; cJSON_Delete(o); return NULL; }
		if (mclib_exists(safe)) { *err = "there's already a card with that name"; cJSON_Delete(o); return NULL; }
		if (!strcmp(op, "new")) {
			mc_blank(img);
			if (store(safe, img, err)) { cJSON_Delete(o); return NULL; }
			mclib_mark_made(safe);
		} else {
			char a[800], b[800];
			if (!good_name(name) || !mclib_exists(name)) { *err = "no such card"; cJSON_Delete(o); return NULL; }
			snprintf(a, sizeof(a), "%s/%s", mclib_dir(), name);
			snprintf(b, sizeof(b), "%s/%s", mclib_dir(), safe);
			if (rename(a, b)) { *err = "couldn't rename it"; cJSON_Delete(o); return NULL; }
		}
		cJSON_AddStringToObject(o, "name", safe);
		plog("memory cards: %s %s", op, safe);
		return o;
	}
	if (!good_name(name) || !mclib_exists(name)) { *err = "no such card"; cJSON_Delete(o); return NULL; }
	if (!strcmp(op, "delete")) {
		if (rm_rf_card(name)) { *err = "couldn't delete it"; cJSON_Delete(o); return NULL; }
		plog("memory cards: deleted %s", name);
		return o;
	}
	if (!strcmp(op, "delversion")) {
		int nv = 0;
		char **v = mclib_versions(name, &nv);
		free_versions(v, nv);
		if (nv < 2) { *err = "a card keeps at least one version: delete the card instead"; cJSON_Delete(o); return NULL; }
		if (!good_name(ver)) { *err = "which version?"; cJSON_Delete(o); return NULL; }
		char p[1200];
		snprintf(p, sizeof(p), "%s/%s/%s%s", mclib_dir(), name, ver, strstr(ver, ".mcd") ? "" : ".mcd");
		if (unlink(p)) { *err = "no such version"; cJSON_Delete(o); return NULL; }
		return o;
	}
	if (!strcmp(op, "import")) {               /* {name, data: base64 .mcs} */
		const char *data = cJSON_GetStringValue(cJSON_GetObjectItem(req, "data"));
		size_t n = 0;
		unsigned char *x = data ? unb64(data, &n) : NULL;
		if (mclib_load(name, NULL, img, NULL, 0) || !x) { free(x); *err = "no such card"; cJSON_Delete(o); return NULL; }
		int r = mc_import(img, x, n, err);
		free(x);
		if (r || store(name, img, err)) { cJSON_Delete(o); return NULL; }
		return o;
	}
	if (!cJSON_IsArray(slots) || !cJSON_GetArraySize(slots)) { *err = "tick the saves first"; cJSON_Delete(o); return NULL; }
	if (mclib_load(name, ver, img, NULL, 0)) { *err = "no such version"; cJSON_Delete(o); return NULL; }
	if (!strcmp(op, "remove")) {
		cJSON *it;
		cJSON_ArrayForEach(it, slots) if (!cJSON_IsNumber(it) || mc_remove(img, it->valueint, err)) { cJSON_Delete(o); return NULL; }
		if (store(name, img, err)) { cJSON_Delete(o); return NULL; }
		return o;
	}
	if (!strcmp(op, "copy") || !strcmp(op, "move")) {
		if (!good_name(to) || !strcmp(to, name)) { *err = "pick another card to copy to"; cJSON_Delete(o); return NULL; }
		if (mclib_load(to, NULL, dst, NULL, 0)) { *err = "no such card to copy to"; cJSON_Delete(o); return NULL; }
		static unsigned char src_after[MC_SIZE];
		memcpy(src_after, img, MC_SIZE);
		unsigned char *x = malloc(128 + 15 * MC_BLOCK);
		cJSON *it;
		cJSON_ArrayForEach(it, slots) {
			size_t n;
			if (!x || !cJSON_IsNumber(it) || mc_export(img, it->valueint, x, &n, err) || mc_import(dst, x, n, err)) { free(x); cJSON_Delete(o); return NULL; }
			if (!strcmp(op, "move") && mc_remove(src_after, it->valueint, err)) { free(x); cJSON_Delete(o); return NULL; }
		}
		free(x);
		if (store(to, dst, err)) { cJSON_Delete(o); return NULL; }
		if (!strcmp(op, "move") && store(name, src_after, err)) { cJSON_Delete(o); return NULL; }
		plog("memory cards: %s %d save(s) %s -> %s", op, cJSON_GetArraySize(slots), name, to);
		return o;
	}
	*err = "op: new, rename, delete, delversion, import, remove, copy or move";
	cJSON_Delete(o);
	return NULL;
}
