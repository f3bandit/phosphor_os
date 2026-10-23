/* Phosphor shell: a terminal window for scripts, inside Phosphor (libvterm on a pseudo-terminal).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_TERM_H
#define PHOS_TERM_H

#include "lvgl.h"

/* a terminal as big as fits in w x h with this (monospaced) font; its canvas */
lv_obj_t *term_create(lv_obj_t *parent, int w, int h, const lv_font_t *font);
void term_forget(void);                              /* its screen was deleted */
int term_start(const char *script, const char *logpath); /* run a script in it; -1 if one runs already */
int term_poll(void);                                 /* every frame: output in, screen drawn; 1 while running */
int term_running(void);
int term_status(void);                               /* exit status of the last script */
void term_key(int vterm_key);                        /* VTERM_KEY_* */
void term_text(const char *s);                       /* typed text */
void term_stop(void);                                /* stop the script */

#endif
