/* Phosphor frontend: settings and the last game played (frontend.ini on the SD card).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_SETTINGS_H
#define PHOS_SETTINGS_H

#define PHOS_VERSION "0.27.41"

typedef struct {
	int accent;
	int clock24;
	int texture;      /* background texture (texture.h), 0 = none */
	int setup_done;   /* first-start setup finished (a new card shows it; existing installs don't) */
	int saver_min;    /* screensaver after this many idle minutes, 0 = never */
	int shot_off;     /* the screenshot combo (L3 + R3) turned off */
	int wallpaper_on; /* Wallpapers on: the picture below instead of the texture (Settings > Wallpapers) */
	int wall_osd;     /* which screens it's for: 0 both, 1 Phosphor's menu only, 2 MiSTer's menu only */
	char wallpaper[600];   /* the picture (under wallpapers/ or wallpapers_user/) */
} settings_t;

extern settings_t SETTINGS;

void settings_load(void);
void settings_save(void);

#endif
