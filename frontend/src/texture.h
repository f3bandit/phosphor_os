/* Phosphor shell: background textures, drawn in the accent colour.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_TEXTURE_H
#define PHOS_TEXTURE_H

#include "lvgl.h"

typedef enum { TEX_NONE, TEX_ZIGZAG, TEX_X, TEX_SQUARES, TEX_TRIANGLES, TEX_CIRCLES, TEX_HEX, TEX_PIXEL, TEX_COUNT } texture_t;

const char *texture_name(texture_t t);
/* draw the texture in the current accent colour as obj's tiled background (TEX_NONE removes it) */
void texture_apply(lv_obj_t *obj, texture_t t);

#endif
