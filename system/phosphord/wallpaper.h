/* phosphord: wallpapers (see wallpaper.c).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_WALLPAPER_H
#define PHOSD_WALLPAPER_H
#include <stddef.h>
#include "cJSON.h"

cJSON *wallpaper_list(const char *category, const char *aspect, const char *current);   /* aspect "16:9", "4:3", "all" */
int wallpaper_thumb(const char *file, char *out, size_t n);         /* 0 made, 1 being made, -1 not a wallpaper */
void wallpaper_prefetch(cJSON *files);                               /* make these next, in this order */
int wallpaper_screen(const char *file, int w, int h, char *out, size_t n);
int wallpaper_osd(const char *file);                                /* "" or NULL: MiSTer's usual */
#endif
