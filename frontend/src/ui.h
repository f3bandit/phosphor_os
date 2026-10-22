/* Phosphor frontend: screens and navigation.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_UI_H
#define PHOS_UI_H

#include "input.h"

void ui_init(int w, int h);
void ui_key(pkey_t k);
void ui_tick(void);          /* every frame: status bar, scan progress, mount changes */
void ui_rescan(void);

#endif
