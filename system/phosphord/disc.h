/* phosphord: a disc in the dock's CD/DVD drive (disc.c).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_DISC_H
#define PHOSD_DISC_H

#include "cJSON.h"
#include "sqlite3.h"
#include <stddef.h>

/* what's in the drive, named and with box art when it can be (see disc.c); NULL: Main has no disc API */
cJSON *disc_json(sqlite3 *db);
int disc_play(sqlite3 *db, char *err, size_t errn);      /* start it on its console */
int disc_eject(char *err, size_t errn);
int disc_enabled(sqlite3 *db);                 /* Settings › Discs */
int disc_set_enabled(sqlite3 *db, int on);     /* also tells Main; -1 Main didn't answer (it's kept anyway) */
/* the "last game" for Play last: the disc when it was played after the last game from storage and
   it's still in the drive (adds "disc" to o and returns 1), else 0 (the storage game, as before) */
int disc_last(sqlite3 *db, cJSON *o, long long last_game_played);

void disc_brief(char *key, size_t n, int *audio);   /* for the music player */

#endif
