/* phosphord: MiSTer & Linux backups for the apps (the system files and the settings files, read and
 * written back in pieces). See sysbk.c for the commands.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_SYSBK_H
#define PHOSD_SYSBK_H
#include "cJSON.h"
cJSON *sysbk_cmd(cJSON *req, const char **err);   /* the reply (no "ok"), or NULL with *err */
#endif
