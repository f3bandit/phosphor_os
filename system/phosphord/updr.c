/* phosphord: Phosphor's own updater, through the updater module (phosphor-modules/updater/updater.py): the
 * catalog's categories and tags (phosphor_os/tools/updater, the Database watch), the user's choices
 * (phosphor/config/updater.ini) and a run with MiSTer's own Downloader, after a restore point. The module
 * prints STEP|, LOG| and RESULT|ok|... or RESULT|fail|... for a run; plan, settings and catalog are JSON.
 * Copyright (C) 2026 f3bandit. All rights reserved; licensed to Retro Remake (see LICENSE.md). */
#include "updr.h"
#include "modjob.h"
#include <stdlib.h>
#include <string.h>

static modjob_t mj = MODJOB("updater", "PHOS_UPDATER", NULL);

int updr_installed(void) { return modjob_installed(&mj); }

char *updr_query(const char *what, int refresh)
{
	int rc = -1;
	const char *a[] = { what, refresh ? "--refresh" : NULL, NULL };
	char *out = modjob_run(&mj, a, 90, &rc);               /* (plan and catalog may download the catalog) */
	if (rc != 0) { free(out); return NULL; }
	return out;
}

int updr_set(const char *section, const char *key, const char *value)
{
	int rc = -1;
	const char *a[] = { "set", section, key, value, NULL };
	free(modjob_run(&mj, a, 20, &rc));
	return rc == 0 ? 0 : -1;
}

int updr_run_start(int dry)
{
	const char *a[] = { "run", dry ? "--dry-run" : NULL, NULL };
	return modjob_start(&mj, dry ? "dry-run" : "run", a, 6 * 3600);   /* (a first full run can take hours) */
}

void updr_last(int *busy, time_t *at, char **out)
{
	char job[24];
	int ok;
	modjob_last(&mj, job, sizeof(job), busy, &ok, out, at);
}
