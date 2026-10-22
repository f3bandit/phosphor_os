/* phosphord: get or update cores with MiSTer's own downloader (Scripts/update.sh), on request.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_CORES_H
#define PHOSD_CORES_H

#include <time.h>

typedef struct {
	int available;        /* MiSTer's updater is there */
	int busy, ok;
	time_t at;            /* when the last run ended, 0 = never */
	char line[200];       /* the downloader's latest line */
} cores_state_t;

void cores_state(cores_state_t *s);
int cores_update(void);   /* background; 1 already running, -1 no updater */

#endif
