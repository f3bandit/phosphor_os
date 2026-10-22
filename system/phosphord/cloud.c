/* phosphord: drives the cloud-sync module (saves to the user's own cloud storage).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Quick things (status, storage places) are asked for on each request; anything that uses the network
 * (setup, send, get back, test, list MiSTers) runs one at a time in the background, stopped after 15
 * minutes, and the screen asks again for its output. What never goes to the cloud is decided by the
 * module itself (WiFi, Bluetooth pairings, passwords, keys are on its fixed list). */
#include "cloud.h"
#include "modjob.h"
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

void cloud_state(cloud_state_t *s)
{
	memset(s, 0, sizeof(*s));
	s->installed = modjob_installed(&mj);
	if (s->installed) {
		int rc;
		const char *st[] = { "status", NULL }, *rm[] = { "remotes", NULL };
		s->status = modjob_run(&mj, st, 20, &rc);
		s->remotes = modjob_run(&mj, rm, 20, &rc);
		if (rc != 0) { free(s->remotes); s->remotes = NULL; }     /* rclone not set up yet */
	}
	modjob_last(&mj, s->job, sizeof(s->job), &s->busy, &s->ok, &s->output, &s->at);
}
