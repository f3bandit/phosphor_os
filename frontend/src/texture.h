/* Phosphor shell: background textures, drawn in the accent colour.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_TEXTURE_H
#define PHOS_TEXTURE_H

#include "lvgl.h"
#include <stdint.h>

/* a texture is a number into the table in texture.c (saved in frontend.ini; 0 = none). The first
 * seven keep their old numbers (5 is retired); the variants come after. */
typedef int texture_t;
#define TEX_NONE 0
extern const int TEX_COUNT;

/* false for numbers out of range or removed (5, the old Circles) */
int texture_valid(texture_t t);
const char *texture_name(texture_t t);
/* the next texture in the order Settings shows them (dir -1 or 1) */
texture_t texture_step(texture_t t, int dir);
/* draw the texture in the current accent colour as obj's tiled background (TEX_NONE removes it) */
void texture_apply(lv_obj_t *obj, texture_t t);
/* the tile texture_apply made (NULL for none); it's replaced the next time texture_apply runs */
const void *texture_tile(void);
/* the tile's coverage (0..255 a pixel) at this scale, for tests and previews: its size in *w, *h;
 * returns NULL for none (caller frees) */
uint8_t *texture_alpha(texture_t t, float scale, int *w, int *h);

#endif
