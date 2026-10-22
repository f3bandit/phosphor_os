/* Phosphor frontend: settings and the last game played (frontend.ini on the SD card).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_SETTINGS_H
#define PHOS_SETTINGS_H

#define PHOS_VERSION "0.7.0"

typedef struct {
	int accent;
	int clock24;
	int texture;      /* background texture (texture.h), 0 = none */
} settings_t;

extern settings_t SETTINGS;

void settings_load(void);
void settings_save(void);

#endif
