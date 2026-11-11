/* phosphord: the dock's IR remote receiver (an RP2040 running DockIR): what it runs, and updating it.
 * Copyright (C) 2026 f3bandit. All rights reserved; licensed to Retro Remake (see LICENSE.md). */
#ifndef PHOSD_DOCKIR_H
#define PHOSD_DOCKIR_H

#include "cJSON.h"

/* {present, firmware ("DIRUP001": updatable from here; "" DockIR without it), version, file (a .uf2 sent
   from the PC, waiting), job {busy, state, message, ok}}; 
   enabled: the on/off setting. brief: without the firmware id (a USB request), for a repeating poll */
cJSON *dockir_json(int brief);
/* the on/off setting (config/dock_ir.off), the receiver switched by phosphord's watcher in the background */
int dockir_enabled(void);
int dockir_set_enabled(int on, char *err, size_t n);
void dockir_start(void);      /* the watcher: keeps the receiver on or off as set, through re-plugs */
/* the .uf2 that was sent (phosphor/updates/dockir.uf2) onto the receiver, in the background: 0 started,
   -1 with err (not present, not updatable, no file, busy) */
int dockir_update_start(char *err, size_t n);
/* a .uf2 file looks right for the receiver (UF2 blocks for the RP2040): 0 yes, -1 with err */
int dockir_check_uf2(const char *path, char *err, size_t n);
const char *dockir_file(void);

#endif
