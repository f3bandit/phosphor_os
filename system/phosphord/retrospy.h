/* phosphord: RetroSpy (see retrospy.c). Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0. */
#ifndef PHOSD_RETROSPY_H
#define PHOSD_RETROSPY_H

#include "cJSON.h"

cJSON *retrospy_cmd(cJSON *req);   /* installed, watching (which jsN), host, login, controllers (js, player) */

#endif
