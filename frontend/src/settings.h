/* Phosphor frontend: settings and the last game played (frontend.ini on the SD card).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_SETTINGS_H
#define PHOS_SETTINGS_H

#include "library.h"

#define PHOS_VERSION "0.1.0"

typedef struct {
	int accent;
	int clock24;
	char last_system[24], last_system_name[48], last_path[1024], last_title[256];
} settings_t;

extern settings_t SETTINGS;

void settings_load(void);
void settings_save(void);
void settings_set_last(const game_system_t *s, const game_t *g);

#endif
