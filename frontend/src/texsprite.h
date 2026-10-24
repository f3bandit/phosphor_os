/* Phosphor shell: sprite textures (pixel-art sprites scattered over the screen or a tile), made with
 * tools/texture-maker. Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_TEXSPRITE_H
#define PHOS_TEXSPRITE_H

#include <stdint.h>

/* one sprite: its size and where its pixels start in the texture's pixel data. The pixels are shades
 * 0..15 (0 = empty), two to a byte (the left one in the high half), each row padded to a byte. */
typedef struct { uint8_t w, h; uint32_t off; } tex_sprite_t;

/* a sprite texture (each one's data is in its own tex_<id>.h) */
typedef struct {
	uint16_t w, h;               /* base pixels (1280x720 = the whole screen; smaller = a tile that repeats) */
	uint8_t px;                  /* base pixels per sprite pixel */
	uint8_t inv;                 /* 1: the accent everywhere except the sprites */
	double str;                  /* how much accent colour a full-shade pixel gets */
	uint16_t nspr;
	const tex_sprite_t *spr;
	const uint8_t *pix;
	uint16_t npl;                /* sprites placed: x, y, sprite, orientation (4 numbers each) */
	const uint16_t *pl;          /* orientation: bits 0-1 turns of 90 degrees clockwise, bit 2 mirrored first,
	                                bit 3 the free angle the other way round (a sprite flipped on screen) */
	int32_t cq, sq;              /* a free angle (under 90 degrees, clockwise) every sprite is then turned by,
	                                about its middle: cosine and sine times 65536 (0, 0 or 65536, 0 = none) */
} tex_sprites_t;

/* the texture's coverage (0..255 a pixel) at scale screen pixels per base pixel; its size in *pw, *ph.
 * The same drawing as the Windows app's and the Texture Maker's (SpriteTex.alpha). Caller frees. */
uint8_t *sprite_alpha(const tex_sprites_t *t, float scale, int *pw, int *ph);

#endif
