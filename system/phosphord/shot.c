/* phosphord: screenshots.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * They live where MiSTer keeps its own: screenshots/<core>/ on the SD card. In a game, MiSTer's Main
 * takes them (Phosphor's Main: D-pad Down + Start, or "screenshot" for the apps); on Phosphor's own
 * screens Main's capture only sees the menu core's picture, so the framebuffer the shell draws on is
 * saved here instead, in screenshots/Phosphor/. The apps list them, see small copies (made once, kept
 * in data/shot_thumbs) and the full picture, and delete them. */
#include "shot.h"
#include "art.h"
#include "log.h"
#include "paths.h"
#include <dirent.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "stb_image_write.h"     /* the implementation is in art.c */

static void shots_dir(char *out, size_t n) { snprintf(out, n, "%s/screenshots", p_fat()); }

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
	char *buf = calloc(1, 4096);
	size_t len = 0;
	while (buf && len < 4095) {
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

/* the framebuffer (what the shell shows) as a PNG */
static int fb_png(const char *out)
{
	int fd = open("/dev/fb0", O_RDONLY | O_CLOEXEC);
	if (fd < 0) return -1;
	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo f;
	if (ioctl(fd, FBIOGET_VSCREENINFO, &v) || ioctl(fd, FBIOGET_FSCREENINFO, &f) || (v.bits_per_pixel != 32 && v.bits_per_pixel != 16) ||
	    v.xres < 16 || v.yres < 16 || v.xres > 4096 || v.yres > 4096) { close(fd); return -1; }
	size_t len = f.smem_len;
	unsigned char *m = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
	close(fd);
	if (m == MAP_FAILED) return -1;
	int w = (int)v.xres, h = (int)v.yres, bpp = (int)v.bits_per_pixel / 8;
	unsigned char *rgb = malloc((size_t)w * h * 3);
	int ok = 0;
	if (rgb && (size_t)(v.yoffset + v.yres) * f.line_length <= len) {
		for (int y = 0; y < h; y++) {
			const unsigned char *row = m + (size_t)(y + v.yoffset) * f.line_length + (size_t)v.xoffset * bpp;
			for (int x = 0; x < w; x++) {
				uint32_t px = bpp == 4 ? *(const uint32_t *)(row + x * 4) : *(const uint16_t *)(row + x * 2);
				unsigned char *o = rgb + ((size_t)y * w + x) * 3;
				const struct fb_bitfield *bf[3] = { &v.red, &v.green, &v.blue };
				for (int c = 0; c < 3; c++) {
					uint32_t val = (px >> bf[c]->offset) & ((1u << bf[c]->length) - 1);
					uint32_t mx = (1u << bf[c]->length) - 1;
					o[c] = (unsigned char)(mx ? val * 255 / mx : 0);
				}
			}
		}
		char tmp[1200];
		snprintf(tmp, sizeof(tmp), "%s.tmp", out);
		ok = stbi_write_png(tmp, w, h, 3, rgb, w * 3) && rename(tmp, out) == 0;
		if (!ok) unlink(tmp);
	}
	free(rgb);
	munmap(m, len);
	return ok ? 0 : -1;
}

int shot_take(char *where, size_t wn, char *file, size_t fn, char *err, size_t errn)
{
	file[0] = 0;
	/* a game running: MiSTer's own (the game's picture, as it does it) */
	char *st = main_req("status", 800);
	/* the game's own picture on the screen (not Phosphor's in-game menu over it, "fb":true) */
	int in_game = st && strstr(st, "\"menu\":false") && !strstr(st, "\"fb\":true");
	free(st);
	if (in_game) {
		char *r = main_req("screenshot", 800);
		int ok = r && strstr(r, "\"ok\":true");
		free(r);
		if (!ok) { snprintf(err, errn, "MiSTer couldn't take it"); return -1; }
		snprintf(where, wn, "game");
		return 0;
	}
	/* Phosphor's own screens */
	char dir[700];
	shots_dir(dir, sizeof(dir));
	mkdir(dir, 0755);
	strncat(dir, "/Phosphor", sizeof(dir) - strlen(dir) - 1);
	mkdir(dir, 0755);
	time_t t = time(NULL);
	struct tm tm;
	localtime_r(&t, &tm);
	char stamp[32];
	strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tm);
	snprintf(file, fn, "%s/%s-phosphor.png", dir, stamp);
	if (fb_png(file)) { snprintf(err, errn, "Couldn't read the screen"); file[0] = 0; return -1; }
	snprintf(where, wn, "phosphor");
	plog("screenshot: %s", file);
	return 0;
}

static int image_name(const char *n)
{
	const char *d = strrchr(n, '.');
	return n[0] != '.' && d && (!strcasecmp(d, ".png") || !strcasecmp(d, ".jpg") || !strcasecmp(d, ".bmp"));
}

typedef struct { char id[300]; char core[100]; long long size; time_t at; } shot_t;
static int shot_cmp(const void *a, const void *b)
{
	const shot_t *x = a, *y = b;
	return x->at < y->at ? 1 : x->at > y->at ? -1 : strcmp(y->id, x->id);
}

cJSON *shot_list(void)
{
	char dir[700];
	shots_dir(dir, sizeof(dir));
	shot_t *l = NULL;
	int n = 0, cap = 0;
	long long bytes = 0;
	DIR *d = opendir(dir);
	struct dirent *e;
	while (d && (e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		char sub[1000];
		snprintf(sub, sizeof(sub), "%s/%s", dir, e->d_name);
		DIR *d2 = opendir(sub);
		struct dirent *f;
		while (d2 && (f = readdir(d2))) {
			if (!image_name(f->d_name)) continue;
			char p[1300];
			snprintf(p, sizeof(p), "%s/%s", sub, f->d_name);
			struct stat sb;
			if (stat(p, &sb) || !S_ISREG(sb.st_mode)) continue;
			if (n == cap) { cap = cap ? cap * 2 : 64; shot_t *nl = realloc(l, (size_t)cap * sizeof(*l)); if (!nl) break; l = nl; }
			snprintf(l[n].id, sizeof(l[n].id), "%.99s/%.190s", e->d_name, f->d_name);
			snprintf(l[n].core, sizeof(l[n].core), "%s", e->d_name);
			l[n].size = (long long)sb.st_size;
			l[n].at = sb.st_mtime;
			bytes += (long long)sb.st_size;
			n++;
		}
		if (d2) closedir(d2);
	}
	if (d) closedir(d);
	if (n) qsort(l, (size_t)n, sizeof(*l), shot_cmp);
	cJSON *o = cJSON_CreateObject(), *a = cJSON_AddArrayToObject(o, "shots");
	for (int i = 0; i < n; i++) {
		cJSON *x = cJSON_CreateObject();
		cJSON_AddStringToObject(x, "id", l[i].id);
		cJSON_AddStringToObject(x, "core", l[i].core);
		cJSON_AddNumberToObject(x, "size", (double)l[i].size);
		cJSON_AddNumberToObject(x, "at", (double)l[i].at);
		cJSON_AddItemToArray(a, x);
	}
	cJSON_AddNumberToObject(o, "total", n);
	cJSON_AddNumberToObject(o, "bytes", (double)bytes);
	free(l);
	return o;
}

/* "<core>/<file>": one folder down in screenshots, a picture, nothing else */
int shot_path(const char *id, char *out, size_t n)
{
	if (!id || !*id || strstr(id, "..") || id[0] == '/' || strchr(id, '\\')) return -1;
	const char *sl = strchr(id, '/');
	if (!sl || sl == id || strchr(sl + 1, '/') || !image_name(sl + 1)) return -1;
	char dir[700];
	shots_dir(dir, sizeof(dir));
	snprintf(out, n, "%s/%s", dir, id);
	struct stat sb;
	return stat(out, &sb) == 0 && S_ISREG(sb.st_mode) ? 0 : -1;
}

static void thumb_of(const char *id, int max, char *out, size_t n)
{
	uint64_t h = 1469598103934665603ULL;
	for (const char *c = id; *c; c++) { h ^= (unsigned char)*c; h *= 1099511628211ULL; }
	snprintf(out, n, "%s/shot_thumbs/%016llx%s.jpg", p_data(), (unsigned long long)h, max > 400 ? "-view" : "");
}

int shot_thumb(const char *id, char *out, size_t n) { return shot_image(id, 360, out, n); }

int shot_image(const char *id, int max, char *out, size_t n)
{
	char src[1100];
	if (shot_path(id, src, sizeof(src))) return -1;
	thumb_of(id, max, out, n);
	struct stat a, b;
	if (!stat(out, &b) && !stat(src, &a) && b.st_mtime >= a.st_mtime) return 0;
	char dir[700];
	snprintf(dir, sizeof(dir), "%s/shot_thumbs", p_data());
	mkdir(dir, 0755);
	return art_thumb(src, NULL, 0, out, max);
}

int shot_delete(const char *id, char *err, size_t errn)
{
	char p[1100], t[800];
	if (shot_path(id, p, sizeof(p))) { snprintf(err, errn, "No such screenshot"); return -1; }
	if (unlink(p)) { snprintf(err, errn, "Couldn't delete it"); return -1; }
	thumb_of(id, 360, t, sizeof(t));
	unlink(t);
	thumb_of(id, 1280, t, sizeof(t));
	unlink(t);
	return 0;
}
