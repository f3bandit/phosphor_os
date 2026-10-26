/* phosphord: starting games and keeping play history.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_PLAY_H
#define PHOSD_PLAY_H

#include "cJSON.h"
#include "sqlite3.h"
#include <stddef.h>

int mister_cmd(const char *cmd, char *err, size_t errn);
int play_launch(sqlite3 *db, long long game_id, char *err, size_t errn);
/* on one of the cores play_cores lists (its index; -1: the usual one) */
int play_launch_core(sqlite3 *db, long long game_id, int core, char *err, size_t errn);
/* the cores this game can start on: [{index, name, core}], the first the usual one; NULL (err) when it can't start */
cJSON *play_cores(sqlite3 *db, long long game_id, char *err, size_t errn);
/* a game started some other way (a disc in the dock's drive, disc.c): count it and its play time on
   this library game (game_id > 0), without making it the "last game" (that stays the last one from storage) */
void play_track(sqlite3 *db, long long game_id);
void play_tick(sqlite3 *db);     /* call every couple of seconds: adds play time when back in the menu */

#endif
