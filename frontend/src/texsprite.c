/* Phosphor shell: sprite textures. Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 * Kept step for step the same as SpriteTex.alpha (apps/windows/web/texture.js and
 * tools/texture-maker/web/spritetex.js), rounding included, so the TV, the Windows app and the
 * Texture Maker draw the same pixels. Built with -ffp-contract=off (CMakeLists.txt): a fused
 * multiply-add rounds differently from JavaScript and could move a pixel at a sprite's edge. */
#include "texsprite.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* JavaScript's Math.round: halves go up */
static int jround(double v) { return (int)floor(v + 0.5); }

static int shade(const tex_sprite_t *s, const uint8_t *pix, int o, int u, int v)
{
	int sw = s->w, sh = s->h, x = u, y = v, w = (o & 1) ? sh : sw;
	for (int r = o & 3; r > 0; r--) {                    /* undo one turn clockwise */
		int nx = y, ny = w - 1 - x;
		x = nx;
		y = ny;
		w = (w == sw) ? sh : sw;
	}
	if (o & 4) x = sw - 1 - x;
	uint8_t b = pix[s->off + (size_t)y * ((sw + 1) >> 1) + (x >> 1)];
	return (x & 1) ? (b & 15) : (b >> 4);
}

uint8_t *sprite_alpha(const tex_sprites_t *t, float scale_f, int *pwp, int *php)
{
	double scale = scale_f;
	int pw = jround(t->w * scale), ph = jround(t->h * scale);
	if (pw < 1 || ph < 1) return NULL;
	uint8_t *m = calloc((size_t)pw * ph, 1);
	if (!m) return NULL;
	int tile = t->w < 1280 || t->h < 720;
	int k = tile ? 1 : 0;
	int32_t cq = t->cq ? t->cq : 65536, sq = t->sq;     /* a header with no angle has 0, 0: none */
	int fr = cq != 65536 || sq != 0;
	double cs = cq / 65536.0, sn0 = sq / 65536.0;
	for (int i = 0; i + 3 < t->npl * 4; i += 4) {
		int x0 = (int16_t)t->pl[i], y0 = (int16_t)t->pl[i + 1], o = t->pl[i + 3];   /* signed: a grid starts off the screen */
		const tex_sprite_t *s = &t->spr[t->pl[i + 2]];
		int ow = (o & 1) ? s->h : s->w, oh = (o & 1) ? s->w : s->h;
		for (int wy = -k; wy <= k; wy++)
			for (int wx = -k; wx <= k; wx++) {
				if (fr) {                                    /* turned by the free angle: each screen pixel looks up the sprite pixel under it */
					double sn = (o & 8) ? -sn0 : sn0, bw = (double)ow * t->px, bh = (double)oh * t->px;
					double cx = x0 + wx * (double)t->w + bw / 2, cy = y0 + wy * (double)t->h + bh / 2;
					double hw = (fabs(bw * cs) + fabs(bh * sn)) / 2, hh = (fabs(bw * sn) + fabs(bh * cs)) / 2;
					int xa = (int)floor((cx - hw) * scale), xb = (int)ceil((cx + hw) * scale);
					int ya = (int)floor((cy - hh) * scale), yb = (int)ceil((cy + hh) * scale);
					if (xa < 0) xa = 0;
					if (ya < 0) ya = 0;
					if (xb > pw) xb = pw;
					if (yb > ph) yb = ph;
					for (int y = ya; y < yb; y++) {
						double dy = (y + 0.5) / scale - cy;
						for (int x = xa; x < xb; x++) {
							double dx = (x + 0.5) / scale - cx;
							int u = (int)floor((dx * cs + dy * sn + bw / 2) / t->px), v = (int)floor((dy * cs - dx * sn + bh / 2) / t->px);
							if (u < 0 || v < 0 || u >= ow || v >= oh) continue;
							int sv = shade(s, t->pix, o, u, v);
							if (sv && m[(size_t)y * pw + x] < sv) m[(size_t)y * pw + x] = (uint8_t)sv;
						}
					}
					continue;
				}
				for (int v = 0; v < oh; v++)
					for (int u = 0; u < ow; u++) {
						int sv = shade(s, t->pix, o, u, v);
						if (!sv) continue;
						double bx = x0 + wx * (double)t->w + u * (double)t->px, by = y0 + wy * (double)t->h + v * (double)t->px;
						int xa = jround(bx * scale), xb = jround((bx + t->px) * scale);
						int ya = jround(by * scale), yb = jround((by + t->px) * scale);
						if (xb <= 0 || yb <= 0 || xa >= pw || ya >= ph) continue;
						if (xa < 0) xa = 0;
						if (ya < 0) ya = 0;
						if (xb > pw) xb = pw;
						if (yb > ph) yb = ph;
						for (int y = ya; y < yb; y++)
							for (int x = xa; x < xb; x++)
								if (m[(size_t)y * pw + x] < sv) m[(size_t)y * pw + x] = (uint8_t)sv;
					}
			}
	}
	for (size_t i = 0; i < (size_t)pw * ph; i++) {
		int c = t->inv ? 15 - m[i] : m[i];
		m[i] = (uint8_t)jround(c * t->str * 255 / 15);
	}
	*pwp = pw;
	*php = ph;
	return m;
}
