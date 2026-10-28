/* phosphord: Phosphor's online updates. Looks at Phosphor's releases on GitHub (one small request,
 * kept 30 min) and, when asked, downloads the newer release package into phosphor/updates, checked
 * against the release's SHA256SUMS.txt. Installing stays the user's choice (update.sh, as for a
 * package put there by hand). Nothing is sent but the request itself.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_UPNET_H
#define PHOSD_UPNET_H
#include "cJSON.h"

/* adds "online" to an update reply: {latest, newer, state, msg, checked}. force: look again now
   (in the background; the reply carries what was last seen) */
void upnet_status(cJSON *o, const char *installed, int force);
/* starts downloading the newest release package (background); 0 = started, else an error */
int upnet_download(const char **err);
/* 1 when a version string a.b.c is newer than b */
int upnet_newer(const char *a, const char *b);
#endif
