/* phosphord: repairs a "soft bricked" Bluetooth (the bt-repair module, SS1 Tool's same script).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_BTFIX_H
#define PHOSD_BTFIX_H

#include <time.h>

int btfix_installed(void);
/* what's wrong, changing nothing: the script's output (malloc'd), NULL when it couldn't run */
char *btfix_check(void);
/* the repair, in the background: 0 started, 1 already running, -1 not installed */
int btfix_repair_start(void);
/* start over with a blank pairings image (MiSTer's S45bluetooth renew): 0 started, 1 busy, -1 not installed */
int btfix_reset_start(void);
/* the last repair: busy, when it ended, its output (malloc'd, caller frees) */
void btfix_last(int *busy, time_t *at, char **out);
/* MiSTer's Bluetooth is being changed (a repair here, or SS1 Tool / Console Mode for Windows): don't pair */
int btfix_busy(void);

#endif
