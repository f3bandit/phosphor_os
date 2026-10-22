/* phosphord: runs the diag module (whole-system checks) in the background and keeps the results.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_DIAG_H
#define PHOSD_DIAG_H

#include <stddef.h>
#include <time.h>

typedef struct {
	int installed;
	int busy;              /* 0 idle, 1 checking, 2 making a report bundle */
	char *output;          /* last --kv output (caller frees), NULL if never run */
	time_t at;             /* when it finished */
	char bundle[600];      /* last report bundle's path */
	char error[200];
} diag_state_t;

int diag_run(void);        /* start the checks; 1 if already busy */
int diag_bundle(void);     /* start a support report (.tar.gz in phosphor/logs) */
void diag_state(diag_state_t *s);
void diag_script(char *out, size_t n);   /* PHOS_DIAG, or <fat>/phosphor/modules/diag/diag.sh */

#endif
