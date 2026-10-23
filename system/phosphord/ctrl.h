/* phosphord: controllers (USB and Bluetooth pads, sticks, fight pads): what's connected, which button
 * layout to draw, what MiSTer maps them with, and backups of the mapping files.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_CTRL_H
#define PHOSD_CTRL_H

#include "cJSON.h"

/* the "controllers" command: see api.c's list */
cJSON *ctrl_command(cJSON *req, const char **err);

#endif
