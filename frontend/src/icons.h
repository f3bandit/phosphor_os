/* Phosphor frontend: outline icons drawn as vectors (our own designs, on a 24 or 72 unit grid).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_ICONS_H
#define PHOS_ICONS_H

#include "lvgl.h"

typedef enum {
	/* 24-unit status icons */
	ICON_SHARE, ICON_ETHERNET, ICON_WIFI, ICON_BLUETOOTH, ICON_UPDATE,
	/* 72-unit category icons */
	ICON_DISC, ICON_CARTRIDGE, ICON_HANDHELD, ICON_ARCADE, ICON_COMPUTER, ICON_PORTS,
	ICON_GEAR, ICON_STAR, ICON_CLOCK, ICON_SEARCH,
	ICON_POWER, ICON_RESTART, ICON_SCREEN, ICON_CHECK, ICON_MUSIC, ICON_OTHER, ICON_UTILITY,
	ICON_COUNT
} icon_t;

/* An icon drawn in the object's text colour. level is for WiFi bars (0-3; -1 = all lit). */
lv_obj_t *icon_create(lv_obj_t *parent, icon_t icon, int size);
void icon_set(lv_obj_t *obj, icon_t icon);
void icon_set_level(lv_obj_t *obj, int level);

#endif
