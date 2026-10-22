/* phosphord: runs a phosphor-modules script, quickly in the foreground or one job at a time in the background.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_MODJOB_H
#define PHOSD_MODJOB_H

#include <pthread.h>
#include <stddef.h>
#include <time.h>

typedef struct {
	const char *module;        /* e.g. "cloud-sync": <fat>/phosphor/modules/<module>/<module>.sh */
	const char *script_env;    /* environment variable that overrides the script path (tests) */
	const char *core_env;      /* the module's variable for /tmp/CORENAME (tests use another file) */
	pthread_mutex_t lk;
	int busy, ok;
	char job[24];
	char *out;
	time_t at;
} modjob_t;

#define MODJOB(mod, senv, cenv) { mod, senv, cenv, PTHREAD_MUTEX_INITIALIZER, 0, 0, "", NULL, 0 }

int modjob_installed(modjob_t *m);
/* run now: output (stdout + stderr, malloc'd), exit code in *rc (-1: not run); killed after limit s */
char *modjob_run(modjob_t *m, const char *const *args, int limit, int *rc);
/* start in the background; 1: busy, -1: not installed / couldn't start */
int modjob_start(modjob_t *m, const char *job, const char *const *args, int limit);
/* last job: copies (output malloc'd, caller frees) */
void modjob_last(modjob_t *m, char *job, size_t jn, int *busy, int *ok, char **out, time_t *at);

#endif
