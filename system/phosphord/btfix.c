/* phosphord: repairs a "soft bricked" Bluetooth (controllers won't pair, pairings gone after every restart,
 * Bluetooth won't switch on) with the bt-repair module: SS1 Tool's own script (scripts/ss1_bt_repair.sh), so
 * Phosphor, SS1 Tool and Console Mode for Windows repair it the same way. What goes wrong and what the repair
 * does is in the script. It prints STEP|, LOG|, FOUND| lines and RESULT|ok|... or RESULT|fail|...
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#include "btfix.h"
#include "modjob.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static modjob_t mj = MODJOB("bt-repair", "PHOS_BTFIX", NULL);

int btfix_installed(void) { return modjob_installed(&mj); }

char *btfix_check(void)
{
	int rc;
	const char *a[] = { "check", NULL };
	return modjob_run(&mj, a, 30, &rc);
}

int btfix_repair_start(void)
{
	const char *a[] = { "repair", NULL };
	return modjob_start(&mj, "repair", a, 180);
}

void btfix_last(int *busy, time_t *at, char **out)
{
	char job[24];
	int ok;
	modjob_last(&mj, job, sizeof(job), busy, &ok, out, at);
}

/* the script, SS1 Tool and Console Mode for Windows write /tmp/ss1_bt.busy (its time) while Bluetooth is
   stopped for a change; older than 5 minutes is left over */
int btfix_busy(void)
{
	int busy = 0;
	time_t at = 0;
	char *o = NULL;
	btfix_last(&busy, &at, &o);
	free(o);
	if (busy) return 1;
	FILE *f = fopen(getenv("PHOS_BT_BUSY") ? getenv("PHOS_BT_BUSY") : "/tmp/ss1_bt.busy", "r");
	if (!f) return 0;
	long t = 0;
	if (fscanf(f, "%ld", &t) != 1) t = 0;
	fclose(f);
	return t > 0 && time(NULL) - t < 300;
}
