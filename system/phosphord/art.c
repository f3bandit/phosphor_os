/* phosphord: the artwork store.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * data/art/<system>/<game folder>/box.jpg. Images are scaled down once, when they're stored, so
 * the shell never has to decode a 2000-pixel scan on the ARM. The game folder name is the game's
 * identity key made file-system safe, so it stays the same when files move between drives. */
#include "art.h"
#include "log.h"
#include "paths.h"
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STB_IMAGE_IMPLEMENTATION
/* box art comes from the internet and from phones: only the formats covers use (less decoder to attack),
 * and no picture bigger than covers ever are (a small file can claim a huge size) */
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_MAX_DIMENSIONS 8192
#define ART_MAX_PIXELS (5000L * 5000L)   /* 100 MB decoded at most */
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "tjpgd.h"

/* a big JPEG decoded at 1/2, 1/4 or 1/8 of its size (TJpgDec's scaled decoding: at 1/8 it only reads
 * each block's DC value), the smallest that's still at least minw x minh: a 4K wallpaper's 320 px
 * thumbnail without decoding all 8 million pixels. RGBA; NULL when it isn't a baseline JPEG (a
 * progressive one goes through stb_image as before) */
struct tj_dev { FILE *f; unsigned char *px; int w; };
static size_t tj_in(JDEC *jd, uint8_t *buf, size_t n)
{
	struct tj_dev *d = jd->device;
	if (buf) return fread(buf, 1, n, d->f);
	return fseek(d->f, (long)n, SEEK_CUR) ? 0 : n;
}
static int tj_out(JDEC *jd, void *bitmap, JRECT *r)
{
	struct tj_dev *d = jd->device;
	const uint8_t *s = bitmap;
	int rw = r->right - r->left + 1;
	for (int y = r->top; y <= r->bottom; y++) {
		unsigned char *o = d->px + ((size_t)y * d->w + r->left) * 4;
		for (int x = 0; x < rw; x++, s += 3, o += 4) { o[0] = s[2]; o[1] = s[1]; o[2] = s[0]; o[3] = 255; }   /* (LVGL's copy gives BGR) */
	}
	return 1;
}
static unsigned char *tj_load_scaled(const char *src, int minw, int minh, int *w, int *h)
{
	FILE *f = fopen(src, "rb");
	if (!f) return NULL;
	unsigned char m[2];
	if (fread(m, 1, 2, f) != 2 || m[0] != 0xFF || m[1] != 0xD8) { fclose(f); return NULL; }
	rewind(f);
	struct tj_dev d = { f, NULL, 0 };
	JDEC jd;
	void *pool = malloc(64 * 1024);
	unsigned char *px = NULL;
	if (pool && jd_prepare(&jd, tj_in, pool, 64 * 1024, &d) == JDR_OK && jd.width > 0 && jd.height > 0
	    && (long)jd.width * jd.height <= ART_MAX_PIXELS) {
		int sc = 0;
		while (sc < 3 && (jd.width >> (sc + 1)) >= minw && (jd.height >> (sc + 1)) >= minh) sc++;
		/* each scaled block is at least a pixel, so the size rounds up */
		d.w = (jd.width + (1 << sc) - 1) >> sc;
		int dh = (jd.height + (1 << sc) - 1) >> sc;
		d.px = calloc((size_t)d.w * dh, 4);
		if (d.px && jd_decomp(&jd, tj_out, (uint8_t)sc) == JDR_OK) {
			px = d.px;
			*w = d.w;
			*h = dh;
		} else free(d.px);
	}
	free(pool);
	fclose(f);
	return px;
}

static void safe_name(const char *key, char *out, size_t n)
{
	size_t j = 0;
	for (const char *p = key; *p && j + 1 < n; p++) {
		unsigned char c = (unsigned char)*p;
		out[j++] = (isalnum(c) || c == '-' || c == '_' || c == '.' || c == ' ' || c >= 0x80) ? (char)c : '_';
	}
	out[j] = 0;
}

void art_path(const char *rel, char *out, size_t n)
{
	snprintf(out, n, "%s/art/%s", p_data(), rel);
}

/* area-average downscale (RGBA): each output pixel is the mean of the input pixels it covers */
static unsigned char *downscale(const unsigned char *in, int w, int h, int nw, int nh)
{
	unsigned char *out = malloc((size_t)nw * nh * 4);
	if (!out) return NULL;
	for (int y = 0; y < nh; y++) {
		int y0 = y * h / nh, y1 = (y + 1) * h / nh;
		if (y1 <= y0) y1 = y0 + 1;
		for (int x = 0; x < nw; x++) {
			int x0 = x * w / nw, x1 = (x + 1) * w / nw;
			if (x1 <= x0) x1 = x0 + 1;
			unsigned long sum[4] = { 0 }, cnt = 0;
			for (int yy = y0; yy < y1; yy++) {
				const unsigned char *row = in + ((size_t)yy * w + x0) * 4;
				for (int xx = x0; xx < x1; xx++, row += 4) {
					sum[0] += row[0]; sum[1] += row[1]; sum[2] += row[2]; sum[3] += row[3];
					cnt++;
				}
			}
			unsigned char *o = out + ((size_t)y * nw + x) * 4;
			for (int c = 0; c < 4; c++) o[c] = (unsigned char)(sum[c] / cnt);
		}
	}
	return out;
}

int art_store_box(const char *system_id, const char *game_key, const char *src, char *rel, size_t reln)
{
	int w, h, n;
	if (!stbi_info(src, &w, &h, &n)) {
		plog("art: can't read %s (%s)", src, stbi_failure_reason());
		return -1;
	}
	if (w < 1 || h < 1 || (long)w * h > ART_MAX_PIXELS) {
		plog("art: %s is %dx%d: too big for box art", src, w, h);
		return -1;
	}
	unsigned char *px = stbi_load(src, &w, &h, &n, 4);
	if (!px) {
		plog("art: can't read %s (%s)", src, stbi_failure_reason());
		return -1;
	}
	int nw = w, nh = h;
	if (w > ART_BOX_MAX || h > ART_BOX_MAX) {
		if (w >= h) { nw = ART_BOX_MAX; nh = (int)((long)h * ART_BOX_MAX / w); }
		else { nh = ART_BOX_MAX; nw = (int)((long)w * ART_BOX_MAX / h); }
		if (nw < 1) nw = 1;
		if (nh < 1) nh = 1;
		unsigned char *s = downscale(px, w, h, nw, nh);
		stbi_image_free(px);
		if (!s) return -1;
		px = s;
	}
	/* JPEG: a 512 px cover is ~60 KB instead of ~500 KB as PNG (a whole library: ~100 MB, not ~1 GB).
	 * Transparent edges are laid on the shell's dark panel colour first. */
	for (long i = 0; i < (long)nw * nh; i++) {
		unsigned char *p = px + i * 4;
		int a = p[3];
		if (a == 255) continue;
		static const int bg[3] = { 24, 24, 28 };
		for (int c = 0; c < 3; c++) p[c] = (unsigned char)((p[c] * a + bg[c] * (255 - a)) / 255);
		p[3] = 255;
	}
	char folder[300], dir[1024], out[1100];
	safe_name(game_key, folder, sizeof(folder));
	snprintf(rel, reln, "%s/%s/box.jpg", system_id, folder);
	snprintf(dir, sizeof(dir), "%s/art/%s/%s", p_data(), system_id, folder);
	mkdirs(dir);
	/* a temporary name of this thread's own: the SS1's run and the app's Local files can store art at once */
	snprintf(out, sizeof(out), "%s/box.jpg.%lx.tmp", dir, (unsigned long)pthread_self());
	int ok = stbi_write_jpg(out, nw, nh, 4, px, 85);
	free(px);
	if (!ok) { plog("art: can't write %s", out); return -1; }
	char final_[1100];
	snprintf(final_, sizeof(final_), "%s/box.jpg", dir);
	rename(out, final_);
	snprintf(final_, sizeof(final_), "%s/box.png", dir);
	unlink(final_);                                  /* from an earlier version */
	return 0;
}

int art_thumb(const char *src, const unsigned char *mem, size_t memlen, const char *out, int max)
{
	int w, h, n, ok;
	unsigned char *px = src ? tj_load_scaled(src, max, 1, &w, &h) : NULL;
	if (!px) {
		ok = src ? stbi_info(src, &w, &h, &n) : stbi_info_from_memory(mem, (int)memlen, &w, &h, &n);
		if (!ok || w < 1 || h < 1 || (long)w * h > ART_MAX_PIXELS) return -1;
		px = src ? stbi_load(src, &w, &h, &n, 4) : stbi_load_from_memory(mem, (int)memlen, &w, &h, &n, 4);
		if (!px) return -1;
	}
	int nw = w, nh = h;
	if (w > max || h > max) {
		if (w >= h) { nw = max; nh = (int)((long)h * max / w); }
		else { nh = max; nw = (int)((long)w * max / h); }
		if (nw < 1) nw = 1;
		if (nh < 1) nh = 1;
		unsigned char *s = downscale(px, w, h, nw, nh);
		free(px);                                   /* (stb_image's and tj_load_scaled's: plain malloc) */
		if (!s) return -1;
		px = s;
	}
	for (long i = 0; i < (long)nw * nh; i++) {
		unsigned char *p = px + i * 4;
		int a = p[3];
		if (a == 255) continue;
		static const int bg[3] = { 24, 24, 28 };
		for (int c = 0; c < 3; c++) p[c] = (unsigned char)((p[c] * a + bg[c] * (255 - a)) / 255);
		p[3] = 255;
	}
	char tmp[1200];
	snprintf(tmp, sizeof(tmp), "%s.tmp", out);
	ok = stbi_write_jpg(tmp, nw, nh, 4, px, 85);
	free(px);
	if (!ok) { unlink(tmp); return -1; }
	return rename(tmp, out) ? -1 : 0;
}

int art_size(const char *src, int *w, int *h)
{
	int n;
	return stbi_info(src, w, h, &n) ? 0 : -1;
}

/* a wallpaper made to cover a W x H screen: scaled (down by averaging, up by nearest pixel), the middle
 * kept, darkened by dim/255 toward the shell's background so the screen's text stays readable. JPEG. */
int art_cover(const char *src, const char *out, int W, int H, int dim)
{
	int w, h, n;
	if (W < 16 || H < 16) return -1;
	unsigned char *px = tj_load_scaled(src, W, H, &w, &h);   /* a 4K JPEG for a 1080p screen: at 1/2 */
	if (!px) {
		if (!stbi_info(src, &w, &h, &n) || w < 1 || h < 1 || (long)w * h > ART_MAX_PIXELS) return -1;
		px = stbi_load(src, &w, &h, &n, 4);
		if (!px) return -1;
	}
	double sc = (double)W / w > (double)H / h ? (double)W / w : (double)H / h;
	int sw = (int)(w * sc + 0.5), sh = (int)(h * sc + 0.5);
	if (sw < W) sw = W;
	if (sh < H) sh = H;
	if (sc < 1.0) {                                     /* down: averaged, then cropped */
		unsigned char *s = downscale(px, w, h, sw, sh);
		free(px);
		if (!s) return -1;
		px = s; w = sw; h = sh;
	}
	unsigned char *o = malloc((size_t)W * H * 3);
	if (!o) { free(px); return -1; }
	int ox = (sw - W) / 2, oy = (sh - H) / 2;
	static const int bg[3] = { 14, 14, 16 };
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			int sx = sc < 1.0 ? x + ox : (int)((x + ox) / sc), sy = sc < 1.0 ? y + oy : (int)((y + oy) / sc);
			if (sx >= w) sx = w - 1;
			if (sy >= h) sy = h - 1;
			const unsigned char *p = px + ((size_t)sy * w + sx) * 4;
			unsigned char *q = o + ((size_t)y * W + x) * 3;
			for (int c = 0; c < 3; c++) {
				int v = (p[c] * p[3] + bg[c] * (255 - p[3])) / 255;
				q[c] = (unsigned char)((v * (255 - dim) + bg[c] * dim) / 255);
			}
		}
	free(px);
	char tmp[1200];
	snprintf(tmp, sizeof(tmp), "%s.tmp", out);
	int ok = stbi_write_jpg(tmp, W, H, 3, o, 88);
	free(o);
	if (!ok) { unlink(tmp); return -1; }
	return rename(tmp, out) ? -1 : 0;
}
