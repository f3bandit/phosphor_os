/* Phosphor frontend: settings and the last game played (frontend.ini on the SD card).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_SETTINGS_H
#define PHOS_SETTINGS_H

#define PHOS_VERSION "0.27.18"

typedef struct {
	int accent;
	int clock24;
	int texture;      /* background texture (texture.h), 0 = none */
	int setup_done;   /* first-start setup finished (a new card shows it; existing installs don't) */
	int saver_min;    /* screensaver after this many idle minutes, 0 = never */   /* the controllers on the front ports move around Phosphor (needs phosphor-main) */
} settings_t;

extern settings_t SETTINGS;

void settings_load(void);
void settings_save(void);

#endif
