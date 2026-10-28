/* phosphord: Phosphor's online updates (see upnet.h).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */

#include "upnet.h"
#include "bgcache.h"
#include "net.h"
#include "paths.h"
#include "sha256.h"
#include "log.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define UPNET_RELEASES "https://api.github.com/repos/f3bandit/phosphor_os/releases?per_page=10"
#define UPNET_TTL (30 * 60)

int upnet_newer(const char *a, const char *b)
{
	if (!a || !b) return 0;
	for (;;) {
		long x = strtol(a, (char **)&a, 10), y = strtol(b, (char **)&b, 10);
		if (x != y) return x > y;
		if (*a != '.' && *b != '.') return 0;
		if (*a == '.') a++;
		if (*b == '.') b++;
		if (!*a && !*b) return 0;
	}
}

/* the newest release that has a Phosphor package: {"version","package","sums","page"} or {"error"} */
static char *upnet_fetch(void *arg)
{
	(void)arg;
	const char *const hdr[] = { "Accept: application/vnd.github+json", "User-Agent: Phosphor (MiSTer)", NULL };
	char *body = NULL;
	size_t len = 0;
	int http = 0;
	int rc = net_request("GET", UPNET_RELEASES, hdr, NULL, &body, &len, &http);
	cJSON *out = cJSON_CreateObject();
	cJSON *rel = rc == 0 && body ? cJSON_Parse(body) : NULL;
	free(body);
	char best[32] = "", pkg[400] = "", sums[400] = "", page[300] = "";
	cJSON *r;
	cJSON_ArrayForEach(r, rel) {
		if (cJSON_IsTrue(cJSON_GetObjectItem(r, "draft"))) continue;
		cJSON *a, *assets = cJSON_GetObjectItem(r, "assets");
		char v[32] = "", p[400] = "", s[400] = "";
		cJSON_ArrayForEach(a, assets) {
			const char *n = cJSON_GetStringValue(cJSON_GetObjectItem(a, "name"));
			const char *u = cJSON_GetStringValue(cJSON_GetObjectItem(a, "browser_download_url"));
			if (!n || !u) continue;
			size_t nl = strlen(n);
			if (!strncmp(n, "phosphor-", 9) && nl > 22 && !strcmp(n + nl - 13, "-armhf.tar.gz") && nl - 22 < sizeof(v)) {
				memcpy(v, n + 9, nl - 22);
				v[nl - 22] = 0;
				snprintf(p, sizeof(p), "%s", u);
			} else if (!strcmp(n, "SHA256SUMS.txt")) snprintf(s, sizeof(s), "%s", u);
		}
		if (v[0] && p[0] && s[0] && (!best[0] || upnet_newer(v, best))) {
			snprintf(best, sizeof(best), "%s", v);
			snprintf(pkg, sizeof(pkg), "%s", p);
			snprintf(sums, sizeof(sums), "%s", s);
			const char *h = cJSON_GetStringValue(cJSON_GetObjectItem(r, "html_url"));
			snprintf(page, sizeof(page), "%s", h ? h : "");
		}
	}
	cJSON_Delete(rel);
	if (best[0]) {
		cJSON_AddStringToObject(out, "version", best);
		cJSON_AddStringToObject(out, "package", pkg);
		cJSON_AddStringToObject(out, "sums", sums);
		cJSON_AddStringToObject(out, "page", page);
	} else cJSON_AddStringToObject(out, "error", rc == 0 ? "no release package found" : "GitHub didn't answer");
	cJSON_AddNumberToObject(out, "checked", (double)time(NULL));
	char *s = cJSON_PrintUnformatted(out);
	cJSON_Delete(out);
	return s;
}
static bgcache_t c_rel = BGCACHE(upnet_fetch, NULL, UPNET_TTL);

/* the download: one at a time */
static pthread_mutex_t dl_lk = PTHREAD_MUTEX_INITIALIZER;
static int dl_running;
static char dl_state[16] = "", dl_msg[200] = "";

static void dl_set(const char *st, const char *msg)
{
	pthread_mutex_lock(&dl_lk);
	snprintf(dl_state, sizeof(dl_state), "%s", st);
	snprintf(dl_msg, sizeof(dl_msg), "%s", msg);
	pthread_mutex_unlock(&dl_lk);
	plog("online update: %s: %s", st, msg);
}

static void *dl_thread(void *arg)
{
	cJSON *r = arg;
	const char *v = cJSON_GetStringValue(cJSON_GetObjectItem(r, "version"));
	const char *pkg = cJSON_GetStringValue(cJSON_GetObjectItem(r, "package"));
	const char *sums = cJSON_GetStringValue(cJSON_GetObjectItem(r, "sums"));
	char dir[300], name[80], dest[420], tmp[440], sp[440], want[65] = "", got[65];
	snprintf(dir, sizeof(dir), "%s/phosphor/updates", p_fat());
	snprintf(name, sizeof(name), "phosphor-%s-armhf.tar.gz", v);
	snprintf(dest, sizeof(dest), "%s/%s", dir, name);
	snprintf(tmp, sizeof(tmp), "%s.download", dest);
	snprintf(sp, sizeof(sp), "%s/.SHA256SUMS-%s", dir, v);
	mkdirs(dir);
	char m[200];
	snprintf(m, sizeof(m), "Downloading Phosphor %s...", v);
	dl_set("working", m);
	int ok = 0;
	if (net_get_file(sums, sp) != 0) dl_set("failed", "Couldn't get the release's checksums");
	else {
		FILE *f = fopen(sp, "r");
		char line[300];
		while (f && fgets(line, sizeof(line), f)) {
			char h[65], n[200];
			if (sscanf(line, "%64s %199s", h, n) == 2 && !strcmp(n, name)) { snprintf(want, sizeof(want), "%s", h); break; }
		}
		if (f) fclose(f);
		unlink(sp);
		if (!want[0]) dl_set("failed", "The release's checksums don't list the package");
		else if (net_get_file(pkg, tmp) != 0) dl_set("failed", "The download didn't finish: check the network");
		else if (sha256_file(tmp, got) != 0 || strcasecmp(got, want)) { unlink(tmp); dl_set("failed", "The download was damaged (checksum): try again"); }
		else if (rename(tmp, dest) != 0) { unlink(tmp); dl_set("failed", "Couldn't save the package on the SD card"); }
		else ok = 1;
	}
	if (ok) {
		snprintf(m, sizeof(m), "Phosphor %s downloaded: install it below", v);
		dl_set("done", m);
	}
	cJSON_Delete(r);
	pthread_mutex_lock(&dl_lk);
	dl_running = 0;
	pthread_mutex_unlock(&dl_lk);
	return NULL;
}

int upnet_download(const char **err)
{
	char *c = bgcache_get(&c_rel, 0);
	cJSON *r = c ? cJSON_Parse(c) : NULL;
	free(c);
	if (!r || !cJSON_GetStringValue(cJSON_GetObjectItem(r, "version"))) {
		cJSON_Delete(r);
		*err = "No newer release seen yet: check again in a moment";
		return -1;
	}
	pthread_mutex_lock(&dl_lk);
	if (dl_running) { pthread_mutex_unlock(&dl_lk); cJSON_Delete(r); *err = "A download is already running"; return -1; }
	dl_running = 1;
	pthread_mutex_unlock(&dl_lk);
	pthread_t t;
	if (pthread_create(&t, NULL, dl_thread, r) != 0) {
		pthread_mutex_lock(&dl_lk); dl_running = 0; pthread_mutex_unlock(&dl_lk);
		cJSON_Delete(r);
		*err = "Couldn't start the download";
		return -1;
	}
	pthread_detach(t);
	return 0;
}

void upnet_status(cJSON *o, const char *installed, int force)
{
	if (force) bgcache_stale(&c_rel);
	char *c = bgcache_get(&c_rel, 0);
	cJSON *r = c ? cJSON_Parse(c) : NULL;
	free(c);
	cJSON *on = cJSON_AddObjectToObject(o, "online");
	const char *v = r ? cJSON_GetStringValue(cJSON_GetObjectItem(r, "version")) : NULL;
	if (v) {
		cJSON_AddStringToObject(on, "latest", v);
		cJSON_AddBoolToObject(on, "newer", installed && upnet_newer(v, installed));
		const char *pg = cJSON_GetStringValue(cJSON_GetObjectItem(r, "page"));
		if (pg) cJSON_AddStringToObject(on, "page", pg);
		/* already downloaded: the package is in phosphor/updates (listed with the others) */
		char p[420];
		snprintf(p, sizeof(p), "%s/phosphor/updates/phosphor-%s-armhf.tar.gz", p_fat(), v);
		cJSON_AddBoolToObject(on, "downloaded", file_exists(p));
	} else if (r && cJSON_GetStringValue(cJSON_GetObjectItem(r, "error")))
		cJSON_AddStringToObject(on, "error", cJSON_GetStringValue(cJSON_GetObjectItem(r, "error")));
	if (r) cJSON_AddNumberToObject(on, "checked", cJSON_GetNumberValue(cJSON_GetObjectItem(r, "checked")));
	cJSON_Delete(r);
	pthread_mutex_lock(&dl_lk);
	if (dl_state[0]) {
		cJSON_AddStringToObject(on, "state", dl_state);
		cJSON_AddStringToObject(on, "msg", dl_msg);
	}
	pthread_mutex_unlock(&dl_lk);
}
