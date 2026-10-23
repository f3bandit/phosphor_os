/* phosphord: the health-watch module's power check.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 * Only on request: the shell asks once after start-up and when Diagnostics opens; nothing on a timer. */
#include "power.h"
#include "modjob.h"
#include <stdlib.h>

static modjob_t mj = MODJOB("health-watch", "PHOS_HEALTH", NULL);

char *power_state(int ack)
{
	if (!modjob_installed(&mj)) return NULL;
	int rc;
	if (ack) {
		const char *a[] = { "power-ack", NULL };
		free(modjob_run(&mj, a, 10, &rc));
	}
	const char *s[] = { "power-state", NULL };
	return modjob_run(&mj, s, 10, &rc);
}
