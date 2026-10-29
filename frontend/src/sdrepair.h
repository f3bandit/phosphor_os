/* Phosphor frontend: the offline SD card repair's screen (phosphor --sdrepair STATE, in the repair copy of
 * MiSTer's Linux only).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_SDREPAIR_H
#define PHOS_SDREPAIR_H

/* the screen, drawn from the repair step's state file and kept up to date from it */
void sdrepair_build(int w, int h, const char *state);

#endif
