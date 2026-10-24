/* phosphord: drives the storage module (SD card, NVMe and USB checks; the faulty overlap tool).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 * Only fsrepair changes a file system (and mount mounts a drive that was left unmounted) (the checker unmounts and repairs it); speed writes and removes
 * one file; the overlap tool is moved into phosphor/backups, never deleted, and can be put back. */
#include "storage.h"
#include "modjob.h"
#include "bgcache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static modjob_t mj = MODJOB("storage", "PHOS_STORAGE", NULL);
static char last_target[64];

/* the drive list takes seconds (each drive's PCIe generation is read): kept, refreshed every 30 s in the
   background and right after a check or mount */
static char *run_targets(void *u) { (void)u; int rc; const char *a[] = { "targets", NULL }; return modjob_run(&mj, a, 30, &rc); }
static char *run_overlap(void *u) { (void)u; int rc; const char *a[] = { "overlap", NULL }; return modjob_run(&mj, a, 15, &rc); }
static bgcache_t c_targets = BGCACHE(run_targets, NULL, 30), c_overlap = BGCACHE(run_overlap, NULL, 60);

static int valid_target(const char *t)
{
	if (!t || !*t || strstr(t, "..")) return 0;
	if (!strcmp(t, "/media/fat")) return 1;
	if (strncmp(t, "/media/usb", 10)) return 0;
	for (const char *p = t + 10; *p; p++) if (*p < '0' || *p > '9') return 0;
	return t[10] != 0;
}

void storage_prime(void) { if (modjob_installed(&mj)) { free(bgcache_get(&c_targets, 0)); free(bgcache_get(&c_overlap, 0)); } }

int storage_start(const char *test, const char *target)
{
	static const char *T[] = { "quick", "windows", "partition", "boot", "kernel", "drive", "health", "speed", "fscheck", "fsrepair", NULL };
	const char *argv[6];
	if (!strcmp(test, "mount")) {                         /* a drive left unmounted: /dev/sdXN */
		if (!target || strncmp(target, "/dev/sd", 7) || target[7] < 'a' || target[7] > 'z' || !target[8]) return -2;
		for (const char *p = target + 8; *p; p++) if (*p < '0' || *p > '9') return -2;
		if (strlen(target) > 11) return -2;
		argv[0] = "mount"; argv[1] = target; argv[2] = NULL;
		snprintf(last_target, sizeof(last_target), "%s", target);
		return modjob_start(&mj, test, argv, 60);
	}
	if (!strcmp(test, "overlap-remove") || !strcmp(test, "overlap-undo")) {
		argv[0] = "overlap"; argv[1] = test + 8; argv[2] = NULL;
		snprintf(last_target, sizeof(last_target), "/media/fat");
		return modjob_start(&mj, test, argv, 60);
	}
	int ok = 0;
	for (int i = 0; T[i]; i++) if (!strcmp(T[i], test)) ok = 1;
	if (!ok) return -2;
	if (!target || !*target) target = "/media/fat";
	if (!valid_target(target)) return -2;
	argv[0] = "run"; argv[1] = test; argv[2] = target; argv[3] = NULL;
	int r = modjob_start(&mj, test, argv, 3600);          /* a full surface read of a big drive takes a while */
	if (r == 0) snprintf(last_target, sizeof(last_target), "%s", target);
	return r;
}

void storage_state(storage_state_t *s)
{
	memset(s, 0, sizeof(*s));
	s->installed = modjob_installed(&mj);
	if (s->installed) {
		s->targets = bgcache_get(&c_targets, 1);
		s->overlap = bgcache_get(&c_overlap, 1);
	}
	modjob_last(&mj, s->job, sizeof(s->job), &s->busy, &s->ok, &s->output, &s->at);
	/* a check or mount just finished: the drives may have changed */
	static time_t seen;
	if (s->at != seen) { seen = s->at; bgcache_stale(&c_targets); bgcache_stale(&c_overlap); }
	snprintf(s->target, sizeof(s->target), "%s", last_target);
}
