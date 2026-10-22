/* phosphord: drives the backup-agent module (snapshots of saves and settings on the SD card, NVMe or NAS).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_BACKUP_H
#define PHOSD_BACKUP_H

#include <time.h>

typedef struct {
	int installed;
	char *status;          /* backup-agent.sh status (key=value lines), caller frees */
	char *list;            /* backup-agent.sh list (name<TAB>size KB), caller frees */
	char job[24];          /* now, preview, restore */
	int busy, ok;
	char *output;
	time_t at;
} backup_state_t;

/* job: now | preview | restore; name: the snapshot; only: e.g. "saves|savestates" or NULL for everything */
int backup_start(const char *job, const char *name, const char *only);
void backup_state(backup_state_t *s);

#endif
