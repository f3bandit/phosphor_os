/* phosphord: drives the update-guard module (settings after a Linux update, restore points).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 * Only ever on request: the screen asks for the state, and its buttons start one job at a time. */
#include "guard.h"
#include "modjob.h"
#include <string.h>

static modjob_t mj = MODJOB("update-guard", "PHOS_GUARD", NULL);

int guard_start(const char *job, const char *const *args, int nargs)
{
	const char *argv[24];
	int a = 0;
	if (nargs < 0 || nargs > 16) return -2;
	if (!strcmp(job, "restore")) { if (!nargs) return -2; argv[a++] = "restore"; }
	else if (!strcmp(job, "keep")) { argv[a++] = "keep"; nargs = 0; }
	else if (!strcmp(job, "rollback")) { if (nargs != 1) return -2; argv[a++] = "rollback"; }
	else if (!strcmp(job, "delete")) { if (!nargs) return -2; argv[a++] = "delete-point"; }
	else return -2;
	for (int i = 0; i < nargs; i++) argv[a++] = args[i];
	argv[a] = NULL;
	return modjob_start(&mj, job, argv, 1800);      /* a rollback may download a Linux release */
}

void guard_state(guard_state_t *s)
{
	memset(s, 0, sizeof(*s));
	s->installed = modjob_installed(&mj);
	if (s->installed) {
		int rc;
		const char *ui[] = { "ui-state", NULL };
		s->ui = modjob_run(&mj, ui, 20, &rc);
	}
	modjob_last(&mj, s->job, sizeof(s->job), &s->busy, &s->ok, &s->output, &s->at);
}
