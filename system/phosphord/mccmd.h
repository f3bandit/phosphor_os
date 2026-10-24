/* phosphord: the memory card library's saves (mccmd.c).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_MCCMD_H
#define PHOSD_MCCMD_H

#include "cJSON.h"

/* memcard do: library | card | export | edit (op ...); NULL and *err on failure */
cJSON *mc_command(cJSON *req, const char **err);

#endif
