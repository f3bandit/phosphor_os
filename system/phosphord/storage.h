/* phosphord: drives the storage module (SD card, NVMe and USB checks; the faulty overlap tool).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_STORAGE_H
#define PHOSD_STORAGE_H

#include <time.h>

typedef struct {
	int installed;
	char *targets;         /* storage.sh targets (path|kind|fs|size_kb|free_kb|model|gen|watts lines), caller frees */
	char *overlap;         /* storage.sh overlap (found=...), caller frees */
	char job[24];          /* the last check: quick, speed, ...; overlap-remove, overlap-undo */
	char target[64];
	int busy, ok;
	char *output;
	time_t at;
} storage_state_t;

/* a check in the background: test quick|windows|partition|boot|kernel|drive|health|speed|fscheck|fsrepair
 * on target (a mount point); or job overlap-remove / overlap-undo. 0 started, 1 busy, -1 not installed, -2 bad */
int storage_start(const char *test, const char *target);
void storage_state(storage_state_t *s);
void storage_prime(void);   /* at start: the drive list worked out in the background */

#endif
