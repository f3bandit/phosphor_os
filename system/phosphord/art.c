/* phosphord: the artwork store.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * data/art/<system>/<game folder>/box.png. Images are scaled down once, when they're stored, so
 * the shell never has to decode a 2000-pixel scan on the ARM. The game folder name is the game's
 * identity key made file-system safe, so it stays the same when files move between drives. */
#include "art.h"
#include "log.h"
#include "paths.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

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
	char folder[300], dir[1024], out[1100];
	safe_name(game_key, folder, sizeof(folder));
	snprintf(rel, reln, "%s/%s/box.png", system_id, folder);
	snprintf(dir, sizeof(dir), "%s/art/%s/%s", p_data(), system_id, folder);
	mkdirs(dir);
	snprintf(out, sizeof(out), "%s/box.png.tmp", dir);
	int ok = stbi_write_png(out, nw, nh, 4, px, nw * 4);
	free(px);
	if (!ok) { plog("art: can't write %s", out); return -1; }
	char final_[1100];
	snprintf(final_, sizeof(final_), "%s/box.png", dir);
	rename(out, final_);
	return 0;
}
