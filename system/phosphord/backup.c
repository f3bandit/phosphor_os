/* phosphord: drives the backup-agent module (snapshots of saves and settings on the SD card, NVMe or NAS).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 * Snapshots stay on local drives or the NAS, so they may hold settings (WiFi); restoring "saves only"
 * leaves those alone. Every restore takes a safety snapshot first (the module's own rule). */
#include "backup.h"
#include "modjob.h"
#include <stdlib.h>
#include <string.h>

static modjob_t mj = MODJOB("backup-agent", "PHOS_BACKUP", "BACKUP_AGENT_CORENAME");

int backup_start(const char *job, const char *name, const char *only)
{
	const char *argv[10];
	int a = 0;
	if (!strcmp(job, "now")) argv[a++] = "now";
	else if (!strcmp(job, "preview") || !strcmp(job, "restore")) {
		if (!name || !*name || strchr(name, '/')) return -2;
		argv[a++] = "restore";
		argv[a++] = name;
		if (only && *only) { argv[a++] = "--only"; argv[a++] = only; }
		if (!strcmp(job, "preview")) argv[a++] = "--dry-run";
	} else return -2;
	argv[a] = NULL;
	return modjob_start(&mj, job, argv, 600);
}

void backup_state(backup_state_t *s)
{
	memset(s, 0, sizeof(*s));
	s->installed = modjob_installed(&mj);
	if (s->installed) {
		int rc;
		const char *st[] = { "status", NULL }, *ls[] = { "list", NULL };
		s->status = modjob_run(&mj, st, 20, &rc);
		s->list = modjob_run(&mj, ls, 20, &rc);
	}
	modjob_last(&mj, s->job, sizeof(s->job), &s->busy, &s->ok, &s->output, &s->at);
}
