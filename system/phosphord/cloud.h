/* phosphord: drives the cloud-sync module (saves to the user's own cloud storage).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_CLOUD_H
#define PHOSD_CLOUD_H

#include <stddef.h>
#include <time.h>

typedef struct {
	int installed;
	char *status;          /* cloud-sync.sh status output, key=value lines (caller frees) */
	char *remotes;         /* cloud-sync.sh remotes output (caller frees) */
	char job[24];          /* last background job: setup, push, pull, preview, test, devices, add, use */
	int busy, ok;
	char *output;          /* its output (caller frees) */
	time_t at;             /* when it finished */
} cloud_state_t;

/* start a job in the background; args are passed to cloud-sync.sh after the job's command.
 * 1: something is already running, -1: not installed / couldn't start */
int cloud_start(const char *job, const char *const *args);
void cloud_state(cloud_state_t *s);

#endif
