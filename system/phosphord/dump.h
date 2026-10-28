/* phosphord: saving a copy of the game disc in the drive (see dump.c).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_DUMP_H
#define PHOSD_DUMP_H
#include <stddef.h>
#include <sqlite3.h>
#include "cJSON.h"

int dump_start(sqlite3 *db, char *err, size_t errn);   /* in the background; -1 and why when it can't */
void dump_stop(void);                                  /* what's unfinished is thrown away */
int dump_busy(void);
int dump_supported(const char *type);                  /* a disc type copies can be saved of */
/* {busy, state, msg, title, system, dest, cue, track, of, data, mb_done, mb_total, percent, rereads, bad,
   verified, seconds, speed, seconds_left} */
cJSON *dump_status(void);
#endif
