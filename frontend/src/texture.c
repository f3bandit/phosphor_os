/* Phosphor shell: background textures, drawn in the accent colour.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Each texture is one small tile, drawn here in code (anti-aliased lines from distance fields), then
 * repeated across the screen by LVGL: drawn once when it's picked, nothing per frame. Faint on
 * purpose: the menu should still read as a dark screen. Variants of each shape: packed closer,
 * bolder lines, filled, or inverted (the colour everywhere but the lines). */
#include "texture.h"
#include "theme.h"
#include "circuit_data.h"
#include "texsprite.h"
#include "tex_mario.h"
#include "tex_zelda.h"
/* sprite textures: includes (tools/texture-maker) */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define STRENGTH 0.13f      /* how much accent colour a line gets */

typedef enum { SH_ZIGZAG, SH_X, SH_SQUARE, SH_TRIANGLE, SH_HEX, SH_TRIGRID, SH_DIAMOND, SH_DIAGRID, SH_GRID, SH_BOARD, SH_SPRITES } shape_t;
enum { M_LINE, M_FILL, M_INVERT };

/* the shape textures leave .spr out (NULL) */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
static const struct {
	const char *name;
	shape_t shape;
	float w, h;          /* tile size in base pixels (1280x720) */
	float line;          /* line width multiplier */
	int mode;
	const tex_sprites_t *spr;   /* SH_SPRITES: its sprites and where they go (tex_<id>.h) */
} T[] = {
	{ "None", 0, 0, 0, 0, 0 },
	{ "Zigzag", SH_ZIGZAG, 16, 16, 1, M_LINE },
	{ "X's", SH_X, 40, 40, 1, M_LINE },
	{ "Squares", SH_SQUARE, 40, 40, 1, M_LINE },
	{ "Triangles", SH_TRIANGLE, 40, 36, 1, M_LINE },
	{ NULL, 0, 0, 0, 0, 0 },                           /* 5 was Circles (removed): falls back to none */
	{ "Hexagons", SH_HEX, 25.980762f, 45, 1, M_LINE },
	{ NULL, 0, 0, 0, 0, 0 },                           /* 7 was the 8-bit tiles (removed): falls back to none */
	/* variants (numbers 8 on) */
	{ "Zigzag, tight", SH_ZIGZAG, 16, 8, 1, M_LINE },
	{ "Zigzag, bold", SH_ZIGZAG, 16, 16, 2.4f, M_LINE },
	{ "X's, packed", SH_X, 14, 14, 1, M_LINE },
	{ "Squares, packed", SH_SQUARE, 17, 17, 1, M_LINE },
	{ "Squares, filled", SH_SQUARE, 26, 26, 1, M_FILL },
	{ "Triangles, packed", SH_TRIGRID, 20, 34.641016f, 1, M_LINE },
	{ "Triangles, bold", SH_TRIANGLE, 40, 36, 2.4f, M_LINE },
	{ "Hexagons, bold", SH_HEX, 25.980762f, 45, 2.4f, M_LINE },
	{ "Honeycomb, inverted", SH_HEX, 25.980762f, 45, 2.0f, M_INVERT },
	{ "Squares, inverted", SH_SQUARE, 17, 17, 1.6f, M_INVERT },
	{ "Diamonds", SH_DIAMOND, 40, 40, 1, M_LINE },
	{ "Diamonds, packed", SH_DIAMOND, 20, 20, 1, M_LINE },
	{ "Diamonds, filled", SH_DIAMOND, 24, 24, 1, M_FILL },
	{ "Diamond lattice", SH_DIAGRID, 20, 20, 1, M_LINE },
	{ "Square grid", SH_GRID, 20, 20, 1, M_LINE },
	{ "Square grid, bold", SH_GRID, 20, 20, 2.4f, M_LINE },
	/* one whole screen, not a repeating tile (circuit_data.h) */
	{ "Circuit board", SH_BOARD, 1280, 720, 1, M_LINE },
	{ "Circuit board, bold", SH_BOARD, 1280, 720, 2, M_LINE },
	/* sprite textures (26 on): pixel-art sprites over the whole screen or a tile, made with
	   tools/texture-maker, which adds them here */
	/* sprite textures: entries (tools/texture-maker) */
};
#pragma GCC diagnostic pop
const int TEX_COUNT = sizeof(T) / sizeof(T[0]);

/* the order Settings steps through: each shape followed by its variants */
static const int ORDER[] = { 0, 1, 8, 9, 2, 10, 3, 11, 12, 17, 22, 23, 18, 19, 20, 21, 4, 13, 14, 6, 15, 16, 24, 25 };

int texture_valid(texture_t t) { return t >= 0 && t < TEX_COUNT && T[t].name; }
const char *texture_name(texture_t t) { return texture_valid(t) ? T[t].name : "None"; }

texture_t texture_step(texture_t t, int dir)
{
	int n = (int)(sizeof(ORDER) / sizeof(ORDER[0])), i = 0;
	while (i < n && ORDER[i] != t) i++;
	if (i >= n) i = 0;
	return ORDER[(i + dir + n) % n];
}

static lv_draw_buf_t *tile;

const void *texture_tile(void) { return tile; }

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

/* signed-ish distance from (x,y) to the shape's outline in a tile w x h (base pixels); *inside is
 * set for the closed shapes (squares, circles) */
static float shape(shape_t s, float x, float y, float w, float h, int *inside)
{
	float cx = w / 2, cy = h / 2;
	*inside = 0;
	switch (s) {
	case SH_ZIGZAG: {                                    /* /\ across the tile, at mid height */
		float m = h / 2, a = fminf(3, h / 3);
		return fminf(seg(x, y, 0, m + a, w / 2, m - a), seg(x, y, w / 2, m - a, w, m + a));
	}
	case SH_X:
		return fminf(seg(x, y, cx - 5, cy - 5, cx + 5, cy + 5), seg(x, y, cx + 5, cy - 5, cx - 5, cy + 5));
	case SH_SQUARE: {
		float dx = fabsf(x - cx), dy = fabsf(y - cy), r = 7, m = fmaxf(dx, dy);
		*inside = m < r;
		return fabsf(m - r);
	}
	case SH_TRIANGLE: {                                  /* outline, pointing up */
		float r = 9;
		float ax = cx, ay = cy - r, bx = cx - r * 0.866f, by = cy + r * 0.5f, qx = cx + r * 0.866f, qy = by;
		return fminf(seg(x, y, ax, ay, bx, by), fminf(seg(x, y, bx, by, qx, qy), seg(x, y, qx, qy, ax, ay)));
	}
	case SH_HEX: {                                       /* honeycomb, pointy-top, R = 15 */
		const float R = 15, W = 1.7320508f * R, H = 3 * R;
		float best = 1e9f;
		for (int j = -1; j <= 2; j++)
			for (int i = -1; i <= 2; i++) {
				float hx = i * W + ((j & 1) ? W / 2 : 0), hy = j * H / 2;
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
	case SH_TRIGRID: {                                   /* triangles packed edge to edge: three line sets */
		float d = fminf(fminf(fabsf(y), fabsf(y - h)), fabsf(y - h / 2));   /* tile is two rows tall */
		/* lines at +-60 degrees through the tile's corners and its bottom middle */
		const float s3 = 0.8660254f;
		for (int k = -2; k <= 2; k++) {
			float ox = k * w;                                /* lines through (ox, h) going up-right / up-left */
			d = fminf(d, fabsf(s3 * (x - ox) + 0.5f * (y - h)));
			d = fminf(d, fabsf(s3 * (x - ox) - 0.5f * (y - h)));
		}
		return d;
	}
	case SH_DIAMOND: {                                   /* a square on its corner */
		float m = fabsf(x - cx) + fabsf(y - cy), r = fminf(9, fminf(w, h) / 2 - 1.5f);
		*inside = m < r;
		return fabsf(m - r) * 0.70710678f;
	}
	case SH_DIAGRID:                                     /* diagonal lines both ways: diamonds edge to edge */
		return 0.70710678f * fminf(fabsf(fmodf(x + y + 4 * w, w) - w / 2), fabsf(fmodf(x - y + 4 * w, w) - w / 2));
	case SH_BOARD:                                       /* drawn by board_alpha */
	case SH_SPRITES:                                     /* drawn by sprite_alpha */
		return 1e9f;
	case SH_GRID:                                        /* squares edge to edge */
		x = fmodf(x + 4 * w, w);
		y = fmodf(y + 4 * h, h);
		return fminf(fminf(x, w - x), fminf(y, h - y));
	}
	return 1e9f;
}

/* the circuit board: traces and vias drawn where they are (each only over its own few pixels), not a
   distance test at every pixel of the screen for every trace */
static void board_line(float *a, int pw, int ph, float s, float ax, float ay, float bx, float by, float half)
{
	int x0 = (int)floorf(fminf(ax, bx) * s - half - 2), x1 = (int)ceilf(fmaxf(ax, bx) * s + half + 2);
	int y0 = (int)floorf(fminf(ay, by) * s - half - 2), y1 = (int)ceilf(fmaxf(ay, by) * s + half + 2);
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > pw) x1 = pw;
	if (y1 > ph) y1 = ph;
	for (int y = y0; y < y1; y++)
		for (int x = x0; x < x1; x++) {
			float d = seg((x + 0.5f) / s, (y + 0.5f) / s, ax, ay, bx, by) * s;
			float v = half + 0.5f - d;
			if (v <= 0) continue;
			if (v > 1) v = 1;
			float *p = &a[(size_t)y * pw + x];
			if (v > *p) *p = v;
		}
}

static void board_via(float *a, int pw, int ph, float s, float cx, float cy, float r_out, float r_hole)
{
	int x0 = (int)floorf((cx - r_out) * s - 2), x1 = (int)ceilf((cx + r_out) * s + 2);
	int y0 = (int)floorf((cy - r_out) * s - 2), y1 = (int)ceilf((cy + r_out) * s + 2);
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > pw) x1 = pw;
	if (y1 > ph) y1 = ph;
	for (int y = y0; y < y1; y++)
		for (int x = x0; x < x1; x++) {
			float dx = (x + 0.5f) / s - cx, dy = (y + 0.5f) / s - cy, r = sqrtf(dx * dx + dy * dy) * s;
			float out = r_out * s + 0.5f - r, in = r - r_hole * s + 0.5f;
			if (out > 1) out = 1;
			if (in > 1) in = 1;
			float v = out < in ? out : in;                /* a ring: filled out to r_out, hollow inside r_hole */
			float *p = &a[(size_t)y * pw + x];
			if (v > 0) *p = fmaxf(*p, v);
			else if (r < r_hole * s) *p = fminf(*p, fmaxf(0, in));   /* the hole stays dark where a trace ends */
		}
}

static uint8_t *board_alpha(float scale, float lw, int pw, int ph)
{
	float *a = calloc((size_t)pw * ph, sizeof(float));
	uint8_t *a8 = malloc((size_t)pw * ph);
	if (!a || !a8) { free(a); free(a8); return NULL; }
	const float C = CIRCUIT_CELL, H = C / 2;
	float half = 0.7f * scale * lw;
	int nt = (int)(sizeof(CIRCUIT_TRACES) / sizeof(CIRCUIT_TRACES[0])) / 2;
	for (int t = 0; t < nt; t++) {
		int first = CIRCUIT_TRACES[t * 2], n = CIRCUIT_TRACES[t * 2 + 1];
		for (int i = first; i + 1 < first + n; i++)
			board_line(a, pw, ph, scale, CIRCUIT_PTS[i * 2] * C + H, CIRCUIT_PTS[i * 2 + 1] * C + H,
			           CIRCUIT_PTS[i * 2 + 2] * C + H, CIRCUIT_PTS[i * 2 + 3] * C + H, half);
	}
	int nv = (int)(sizeof(CIRCUIT_VIAS) / sizeof(CIRCUIT_VIAS[0])) / 2;
	for (int i = 0; i < nv; i++)
		board_via(a, pw, ph, scale, CIRCUIT_VIAS[i * 2] * C + H, CIRCUIT_VIAS[i * 2 + 1] * C + H, 2.8f + (lw - 1) * 0.6f, 1.5f);
	for (size_t i = 0; i < (size_t)pw * ph; i++) a8[i] = (uint8_t)lroundf(a[i] * STRENGTH * 255);
	free(a);
	return a8;
}

uint8_t *texture_alpha(texture_t t, float scale, int *pw, int *ph)
{
	if (t <= TEX_NONE || !texture_valid(t)) return NULL;
	if (T[t].shape == SH_SPRITES)
		return T[t].spr ? sprite_alpha(T[t].spr, scale, pw, ph) : NULL;
	if (T[t].shape == SH_BOARD) {
		*pw = (int)lroundf(T[t].w * scale);
		*ph = (int)lroundf(T[t].h * scale);
		return board_alpha(scale, T[t].line, *pw, *ph);
	}
	float bw = T[t].w, bh = T[t].h;
	*pw = (int)lroundf(bw * scale);
	*ph = (int)lroundf(bh * scale);
	if (*pw < 4 || *ph < 4) return NULL;
	uint8_t *a8 = malloc((size_t)*pw * (size_t)*ph);
	if (!a8) return NULL;
	float half = 0.55f * scale * T[t].line;              /* line half width */
	for (int y = 0; y < *ph; y++)
		for (int x = 0; x < *pw; x++) {
			/* sample the base-size tile; wrap so lines crossing the edge join up */
			/* (by the rounded tile size, so a tile whose base size isn't a whole number of pixels
			   still meets its neighbours without a seam) */
			float fx = (x + 0.5f) * bw / *pw, fy = (y + 0.5f) * bh / *ph, d = 1e9f;
			int in = 0, any_in = 0;
			for (int oy = -1; oy <= 1; oy++)
				for (int ox = -1; ox <= 1; ox++) {
					float dd = shape(T[t].shape, fx + ox * bw, fy + oy * bh, bw, bh, &in);
					if (dd < d) d = dd;
					if (in && ox == 0 && oy == 0) any_in = 1;
				}
			d *= (float)*pw / bw;
			float line = half + 0.5f - d;                 /* 1 px of soft edge */
			if (line < 0) line = 0;
			if (line > 1) line = 1;
			float a;
			if (T[t].mode == M_FILL) a = any_in ? 1.0f : line;
			else if (T[t].mode == M_INVERT) a = 1.0f - line;
			else a = line;
			float strength = T[t].mode == M_LINE ? STRENGTH : STRENGTH * 0.75f;   /* areas read stronger than lines */
			a8[(size_t)y * *pw + x] = (uint8_t)lroundf(a * strength * 255);
		}
	return a8;
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
	float scale = S(1000) / 1000.0f;                    /* sizes are for 1280x720 */
	if (scale < 0.5f) scale = 0.5f;
	int pw, ph;
	uint8_t *a8 = texture_alpha(t, scale, &pw, &ph);
	if (!a8) return;
	tile = lv_draw_buf_create((uint32_t)pw, (uint32_t)ph, LV_COLOR_FORMAT_ARGB8888, 0);
	if (!tile) { free(a8); return; }
	lv_color_t c = PAL.acc;
	for (int y = 0; y < ph; y++) {
		uint8_t *row = tile->data + (size_t)y * tile->header.stride;
		for (int x = 0; x < pw; x++) {
			row[x * 4 + 0] = c.blue;
			row[x * 4 + 1] = c.green;
			row[x * 4 + 2] = c.red;
			row[x * 4 + 3] = a8[(size_t)y * pw + x];
		}
	}
	free(a8);
	lv_obj_set_style_bg_image_src(obj, tile, 0);
	lv_obj_set_style_bg_image_tiled(obj, true, 0);
	lv_obj_set_style_bg_image_opa(obj, LV_OPA_COVER, 0);
}
