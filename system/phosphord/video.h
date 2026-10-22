/* phosphord: screen filters per system (video filter, scanlines, shadow mask, gamma, presets).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_VIDEO_H
#define PHOSD_VIDEO_H

#include <stddef.h>

typedef struct {
	char core[64];
	int flt_on[4];            /* filter (horizontal), vertical, scanlines, interlaced */
	char flt[4][256];
	int mask_mode;            /* 0 off, 1 1x, 2 2x, 3 1x rotated, 4 2x rotated */
	char mask[256];
	int gamma_on;
	char gamma[256];
	int custom;               /* this core has its own settings (else MiSTer.ini's defaults apply) */
} video_state_t;

typedef void (*video_list_cb)(void *u, const char *relpath);

int video_core_name(const char *system_id, char *out, size_t n);   /* -1: no core / arcade */
void video_get(const char *core, video_state_t *s);
/* each change backs up the core's three files first; it shows next time the core starts */
int video_set(const char *core, const char *what, const char *value, char *err, size_t errn);
int video_preset(const char *core, const char *preset, char *err, size_t errn);
int video_undo(const char *core, char *err, size_t errn);
int video_list(const char *kind, video_list_cb cb, void *u);       /* filters, masks, gamma, presets */

#endif
