/* Phosphor frontend: screens and navigation.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_UI_H
#define PHOS_UI_H

#include "input.h"

void ui_init(int w, int h);
void ui_key(pkey_t k);
/* how long the main loop may sleep (ms) when nothing comes in: short only while a screen needs it */
int ui_max_sleep(void);
/* a script run through Main is done (its exit status): Phosphor has the screen again */
void ui_script_done(int status);
void ui_tick(void);          /* every frame: status bar, scan progress, mount changes */
void ui_rescan(void);

void ui_screensaver_now(void);   /* show the screensaver right away (snapshots) */
void ui_activity(void);          /* back from a game or MiSTer's menu: the idle time starts again */

#endif
