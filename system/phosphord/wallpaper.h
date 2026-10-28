/* phosphord: wallpapers (see wallpaper.c).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_WALLPAPER_H
#define PHOSD_WALLPAPER_H
#include <stddef.h>
#include "cJSON.h"

cJSON *wallpaper_list(const char *category, const char *aspect);   /* aspect "16:9", "4:3" or "all" */
int wallpaper_thumb(const char *file, char *out, size_t n);
int wallpaper_screen(const char *file, int w, int h, char *out, size_t n);
int wallpaper_osd(const char *file);                                /* "" or NULL: MiSTer's usual */
#endif
