/* phosphord: Phosphor's own updater (phosphor-modules/updater): what Update All installs, from Phosphor's
 * catalog, with MiSTer's own Downloader.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_UPDR_H
#define PHOSD_UPDR_H

#include <time.h>

int updr_installed(void);
/* the module's output now (malloc'd): "plan", "settings", "catalog" (JSON); NULL when it didn't run */
char *updr_query(const char *what, int refresh);
/* the user's choice: section categories|databases|allow|options; 0 done */
int updr_set(const char *section, const char *key, const char *value);
/* a run in the background (dry: only the Downloader's ini); 1 busy, -1 couldn't start */
int updr_run_start(int dry);
void updr_last(int *busy, time_t *at, char **out);

#endif
