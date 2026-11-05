/* phosphord: the dock's IR remote receiver (an RP2040 running DockIR): what it runs, and updating it.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_DOCKIR_H
#define PHOSD_DOCKIR_H

#include "cJSON.h"

/* {present, firmware ("DIRUP001": updatable from here; "" DockIR without it), version, file (a .uf2 sent
   from the PC, waiting), job {busy, state, message, ok}} */
cJSON *dockir_json(void);
/* the .uf2 that was sent (phosphor/updates/dockir.uf2) onto the receiver, in the background: 0 started,
   -1 with err (not present, not updatable, no file, busy) */
int dockir_update_start(char *err, size_t n);
/* a .uf2 file looks right for the receiver (UF2 blocks for the RP2040): 0 yes, -1 with err */
int dockir_check_uf2(const char *path, char *err, size_t n);
const char *dockir_file(void);

#endif
