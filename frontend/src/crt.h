/* Phosphor shell: CRT mode, Phosphor's frames out of the analog port at a TV's own timing (see crt.c).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_CRT_H
#define PHOS_CRT_H
#include "lvgl.h"

/* the modes Phosphor offers (the menu core's native modes: rtl/native_video_*, phosphor-menu) */
enum { CRT_OFF = -1, CRT_480I = 1, CRT_576I = 4, CRT_480P = 7 };

typedef struct { int on, mode, h, v; } crt_cfg_t;

int crt_load(crt_cfg_t *c);                         /* phosphor/config/crt (Phosphor's Main keeps it); 0: read */
int crt_save(const crt_cfg_t *c);                   /* (Main writes it too, when told) */
int crt_size(int mode, int *w, int *h);             /* the frame size of a mode; -1: not one Phosphor offers */
const char *crt_mode_name(int mode);                /* "480i (15 kHz)" */
/* CRT mode on: Phosphor's frames also go to the native buffer, at the display's size */
int crt_attach(lv_display_t *d, int w, int h);
int crt_active(void);
/* the shell starts again (main.c) to take a new CRT setting: the screen size changes with it */
void crt_request_restart(void);
int crt_take_restart(void);
/* trying a mode: the old one comes back by itself unless kept (a TV that can't show it shows nothing) */
#define CRT_KEEP_SECS 20
int crt_try(const crt_cfg_t *nc);                   /* saves it, tells Main, and the shell starts again */
int crt_pending(crt_cfg_t *prev);                   /* 1: a try is waiting for Keep (prev: what comes back) */
void crt_keep(void);
void crt_revert(void);                              /* the old mode back (the shell starts again) */
int crt_move(int h, int v);                         /* the centering, now (no restart) */
#endif
