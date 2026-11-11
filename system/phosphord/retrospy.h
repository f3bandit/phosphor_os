/* phosphord: RetroSpy (see retrospy.c). Copyright (C) 2026 f3bandit. All rights reserved; licensed to Retro Remake. */
#ifndef PHOSD_RETROSPY_H
#define PHOSD_RETROSPY_H

#include "cJSON.h"

cJSON *retrospy_cmd(cJSON *req);   /* installed, watching (which jsN), host, login, controllers (js, player) */

#endif
