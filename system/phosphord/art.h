/* phosphord: the artwork store.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_ART_H
#define PHOSD_ART_H

#include <stddef.h>

#define ART_BOX_MAX 512   /* longest side of stored box art, in pixels */

/* Store an image as a game's box art: decoded (PNG, JPEG, GIF, BMP...), scaled down to fit
 * ART_BOX_MAX, saved as PNG. rel receives the path relative to data/art. Returns 0 on success. */
int art_store_box(const char *system_id, const char *game_key, const char *src_image, char *rel, size_t reln);

/* absolute path of a stored image */
void art_path(const char *rel, char *out, size_t n);

#endif
