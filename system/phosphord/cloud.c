/* phosphord: drives the cloud-sync module (saves to the user's own cloud storage).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Quick things (status, storage places) are asked for on each request; anything that uses the network
 * (setup, send, get back, test, list MiSTers) runs one at a time in the background, stopped after 15
 * minutes, and the screen asks again for its output. What never goes to the cloud is decided by the
 * module itself (WiFi, Bluetooth pairings, passwords, keys are on its fixed list). */
#include "cloud.h"
#include "modjob.h"
#include "bgcache.h"
#include <stdlib.h>
#include <string.h>

static modjob_t mj = MODJOB("cloud-sync", "PHOS_CLOUD", "CLOUD_SYNC_CORENAME");

int cloud_start(const char *j, const char *const *args)
{
	const char *argv[26];
	int a = 0;
	argv[a++] = !strcmp(j, "preview") ? "pull" : j;        /* "preview" is a dry-run pull */
	for (int i = 0; args && args[i] && a < 24; i++) argv[a++] = args[i];
	if (!strcmp(j, "preview")) argv[a++] = "--dry-run";
	argv[a] = NULL;
	return modjob_start(&mj, j, argv, 900);
}

/* rclone's answers take a couple of seconds: kept, refreshed in the background (fresh after a job) */
static char *run_status(void *u) { (void)u; int rc; const char *a[] = { "status", NULL }; return modjob_run(&mj, a, 20, &rc); }
static char *run_remotes(void *u)
{
	(void)u;
	int rc;
	const char *a[] = { "remotes", NULL };
	char *r = modjob_run(&mj, a, 20, &rc);
	if (rc != 0) { free(r); r = NULL; }               /* rclone not set up yet */
	return r;
}
static bgcache_t c_status = BGCACHE(run_status, NULL, 15), c_remotes = BGCACHE(run_remotes, NULL, 15);

static int fresh_next;
void cloud_fresh(void) { fresh_next = 1; }   /* after a change: the next answer is worked out there and then */

void cloud_state(cloud_state_t *s)
{
	memset(s, 0, sizeof(*s));
	s->installed = modjob_installed(&mj);
	if (s->installed) {
		if (fresh_next) { s->status = bgcache_now(&c_status); s->remotes = bgcache_now(&c_remotes); }
		else { s->status = bgcache_get(&c_status, 1); s->remotes = bgcache_get(&c_remotes, 1); }
		fresh_next = 0;
	}
	modjob_last(&mj, s->job, sizeof(s->job), &s->busy, &s->ok, &s->output, &s->at);
	static time_t seen;
	static int was_busy;
	if (s->at != seen || s->busy != was_busy) { seen = s->at; was_busy = s->busy; bgcache_stale(&c_status); bgcache_stale(&c_remotes); }
}
