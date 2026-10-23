/* Phosphor shell: background textures, drawn in the accent colour.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Each texture is one small tile, drawn here in code (anti-aliased lines from distance fields), then
 * repeated across the screen by LVGL. Faint on purpose: the menu should still read as a dark screen.
 * The 8-bit tiles are original pixel art drawn for Phosphor, not graphics from any game. */
#include "texture.h"
#include "theme.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char *NAMES[TEX_COUNT] = { "None", "Zigzag", "X's", "Squares", "Triangles", "Circles", "Hexagons" };
const char *texture_name(texture_t t) { return t < TEX_COUNT ? NAMES[t] : "?"; }

#define STRENGTH 0.13f      /* how much accent colour a line gets */

static lv_draw_buf_t *tile;

static float seg(float px, float py, float ax, float ay, float bx, float by)
{
	float vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay;
	float l2 = vx * vx + vy * vy;
	float t = l2 > 0 ? (wx * vx + wy * vy) / l2 : 0;
	if (t < 0) t = 0;
	if (t > 1) t = 1;
	float dx = wx - t * vx, dy = wy - t * vy;
	return sqrtf(dx * dx + dy * dy);
}

/* distance from (x,y) to the shape, in a tile w x h (units: base pixels at 1280x720) */
static float shape(texture_t t, float x, float y, float w, float h)
{
	float cx = w / 2, cy = h / 2;
	switch (t) {
	case TEX_ZIGZAG: {                                   /* tile 16 x 16: /\/\ */
		float d = seg(x, y, 0, 11, 8, 5);
		d = fminf(d, seg(x, y, 8, 5, 16, 11));
		return d;
	}
	case TEX_X: {                                        /* tile 40: an x in the middle */
		float d = fminf(seg(x, y, cx - 5, cy - 5, cx + 5, cy + 5), seg(x, y, cx + 5, cy - 5, cx - 5, cy + 5));
		return d;
	}
	case TEX_SQUARES: {
		float dx = fabsf(x - cx), dy = fabsf(y - cy), r = 7;
		return fabsf(fmaxf(dx, dy) - r);
	}
	case TEX_TRIANGLES: {                                /* outline, pointing up */
		float r = 9;
		float ax = cx, ay = cy - r, bx = cx - r * 0.866f, by = cy + r * 0.5f, qx = cx + r * 0.866f, qy = by;
		return fminf(seg(x, y, ax, ay, bx, by), fminf(seg(x, y, bx, by, qx, qy), seg(x, y, qx, qy, ax, ay)));
	}
	case TEX_CIRCLES: {
		float dx = x - cx, dy = y - cy;
		return fabsf(sqrtf(dx * dx + dy * dy) - 7);
	}
	case TEX_HEX: {                                      /* honeycomb, pointy-top, R = 15 */
		const float R = 15, W = 1.7320508f * R, H = 3 * R;
		(void)w; (void)h;
		float best = 1e9f;
		/* centres of the hexes around this point */
		for (int j = -1; j <= 2; j++)
			for (int i = -1; i <= 2; i++) {
				float hx = i * W + ((j & 1) ? W / 2 : 0), hy = j * H / 2;
				/* edges of a pointy-top hex */
				float vx[6], vy[6];
				for (int k = 0; k < 6; k++) {
					float a = (float)(M_PI / 180.0) * (60.0f * k - 30.0f);
					vx[k] = hx + R * cosf(a);
					vy[k] = hy + R * sinf(a);
				}
				for (int k = 0; k < 6; k++) best = fminf(best, seg(x, y, vx[k], vy[k], vx[(k + 1) % 6], vy[(k + 1) % 6]));
			}
		return best;
	}
	default: return 1e9f;
	}
}

/* 8-bit tiles: original 16 x 16 pixel art (a little gem and dots), drawn 2x */
static void tile_size(texture_t t, float *w, float *h)
{
	switch (t) {
	case TEX_ZIGZAG: *w = 16; *h = 16; break;
	case TEX_TRIANGLES: *w = 40; *h = 36; break;
	case TEX_HEX: *w = 1.7320508f * 15; *h = 45; break;
	default: *w = 40; *h = 40; break;
	}
}

static void free_tile(void)
{
	if (tile) lv_draw_buf_destroy(tile);
	tile = NULL;
}

void texture_apply(lv_obj_t *obj, texture_t t)
{
	lv_obj_set_style_bg_image_src(obj, NULL, 0);
	free_tile();
	if (t == TEX_NONE || t >= TEX_COUNT) return;
	float scale = S(1000) / 1000.0f;                    /* sizes are for 1280x720 */
	if (scale < 0.5f) scale = 0.5f;
	float bw, bh;
	tile_size(t, &bw, &bh);
	int pw = (int)lroundf(bw * scale), ph = (int)lroundf(bh * scale);
	if (pw < 4 || ph < 4) return;
	tile = lv_draw_buf_create((uint32_t)pw, (uint32_t)ph, LV_COLOR_FORMAT_ARGB8888, 0);
	if (!tile) return;
	lv_color_t c = PAL.acc;
	float half = 0.55f * scale;                          /* line half width */
	for (int y = 0; y < ph; y++) {
		uint8_t *row = tile->data + (size_t)y * tile->header.stride;
		for (int x = 0; x < pw; x++) {
			float a = 0;
			/* sample the base-size tile; wrap so lines crossing the edge join up */
			float fx = (x + 0.5f) / scale, fy = (y + 0.5f) / scale, d = 1e9f;
			for (int oy = -1; oy <= 1; oy++)
				for (int ox = -1; ox <= 1; ox++) d = fminf(d, shape(t, fx + ox * bw, fy + oy * bh, bw, bh));
			d *= scale;
			a = half + 0.5f - d;                         /* 1 px of soft edge */
			if (a < 0) a = 0;
			if (a > 1) a = 1;
			uint8_t al = (uint8_t)lroundf(a * STRENGTH * 255);
			row[x * 4 + 0] = c.blue;
			row[x * 4 + 1] = c.green;
			row[x * 4 + 2] = c.red;
			row[x * 4 + 3] = al;
		}
	}
	lv_obj_set_style_bg_image_src(obj, tile, 0);
	lv_obj_set_style_bg_image_tiled(obj, true, 0);
	lv_obj_set_style_bg_image_opa(obj, LV_OPA_COVER, 0);
}
