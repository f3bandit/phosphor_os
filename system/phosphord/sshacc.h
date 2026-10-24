/* phosphord: SSH modes, SSH keys and the kept copy of the remote access setup (sshacc.c).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_SSHACC_H
#define PHOSD_SSHACC_H

#include "cJSON.h"
#include <stddef.h>

void sshacc_boot(void);             /* before remote_start: a fresh card gets the kept copy back; then apply */
void sshacc_tick(void);             /* the main loop: saves the kept copy a moment after a change */
void sshacc_changed(void);          /* pairing changed (remote.c): the copy needs saving */
cJSON *sshacc_json(int places);     /* mode, keys, pending offers, the copy; places: drives for it */
/* the MiSTer's own changes; NULL ok, else the error. msg: a note (keys only fell back, say) */
const char *sshacc_do(cJSON *req, char *msg, size_t mn);
/* keys offered by a paired device: wait for a yes on the TV; how many were taken */
int sshacc_offer(cJSON *keys, const char *from, const char **err);

#endif
