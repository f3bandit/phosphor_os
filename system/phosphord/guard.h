/* phosphord: drives the update-guard module (settings after a Linux update, restore points).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_GUARD_H
#define PHOSD_GUARD_H

#include <time.h>

typedef struct {
	int installed;
	char *ui;              /* update-guard.sh ui-state (key=value lines), caller frees */
	char job[24];          /* restore, keep, rollback, delete */
	int busy, ok;
	char *output;
	time_t at;
} guard_state_t;

/* job: restore (args: the files), keep, rollback (args: one point), delete (args: points).
 * 0 started, 1 busy, -1 not installed, -2 bad request */
int guard_start(const char *job, const char *const *args, int nargs);
void guard_state(guard_state_t *s);

#endif
