/* phosphord: Update All from Phosphor (see ua.c). Copyright (C) 2026 f3bandit. All rights reserved; licensed to Retro Remake. */
#ifndef PHOSD_UA_H
#define PHOSD_UA_H

#include "cJSON.h"

int ua_installed(void);
/* the "update_all" command: {"do":"list"|"act"|"allow"|"preview"|"uninstall","args":[...]}; NULL + *err on failure */
cJSON *ua_cmd(cJSON *req, const char **err);
/* now and then (the main loop): stops the bridge after 5 minutes unused */
void ua_tick(void);

#endif
