/* phosphord: controllers (USB and Bluetooth pads, sticks, fight pads): what's connected, which button
 * layout to draw, what MiSTer maps them with, and backups of the mapping files.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_CTRL_H
#define PHOSD_CTRL_H

#include "cJSON.h"

/* the "controllers" command: see api.c's list */
cJSON *ctrl_command(cJSON *req, const char **err);

/* for RetroSpy: one controller's (event device's) layout key, its vid:pid, and each control's input on its jsN
 * device ("binds": {"a":{"b":0}, "lsx":{"a":0}, "up":{"a":7,"half":-1}, ...}); NULL when it isn't a controller */
cJSON *ctrl_retrospy(const char *ev);

#endif
