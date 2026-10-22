/* phosphord: starting games and keeping play history.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_PLAY_H
#define PHOSD_PLAY_H

#include "sqlite3.h"
#include <stddef.h>

int mister_cmd(const char *cmd, char *err, size_t errn);
int play_launch(sqlite3 *db, long long game_id, char *err, size_t errn);
void play_tick(sqlite3 *db);     /* call every couple of seconds: adds play time when back in the menu */

#endif
