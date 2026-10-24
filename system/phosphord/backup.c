/* phosphord: drives the backup-agent module (snapshots of saves and settings on the SD card, NVMe or NAS).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 * Snapshots stay on local drives or the NAS, so they may hold settings (WiFi); restoring "saves only"
 * leaves those alone. Every restore takes a safety snapshot first (the module's own rule). */
#include "backup.h"
#include "modjob.h"
#include "bgcache.h"
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

/* the module's status and snapshot list take half a second: kept, refreshed in the background, and
   worked out there and then when a backup or restore starts or ends */
static char *run1(void *what) { int rc; const char *a[] = { what, NULL }; return modjob_run(&mj, a, 20, &rc); }
static bgcache_t c_status = BGCACHE(run1, "status", 20), c_list = BGCACHE(run1, "list", 20);

void backup_state(backup_state_t *s)
{
	memset(s, 0, sizeof(*s));
	s->installed = modjob_installed(&mj);
	modjob_last(&mj, s->job, sizeof(s->job), &s->busy, &s->ok, &s->output, &s->at);
	static time_t seen;
	static int was_busy;
	int changed = s->at != seen || s->busy != was_busy;
	seen = s->at; was_busy = s->busy;
	if (s->installed) {
		s->status = changed ? bgcache_now(&c_status) : bgcache_get(&c_status, 1);
		s->list = changed ? bgcache_now(&c_list) : bgcache_get(&c_list, 1);
	}
}
