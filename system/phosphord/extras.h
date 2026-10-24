/* phosphord: what the Windows app's Devices, Network and Advanced sections read (and MiSTer.ini, scripts).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_EXTRAS_H
#define PHOSD_EXTRAS_H
#include "cJSON.h"
/* each returns the reply object (no "ok"), or NULL with *err set */
cJSON *ex_network(void);
cJSON *ex_usb(void);
cJSON *ex_ini(cJSON *req, const char **err);
cJSON *ex_scripts(cJSON *req, const char **err);
#endif
