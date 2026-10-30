/* phosphord: finds games on the SD card, USB drives and the network share.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_INDEXER_H
#define PHOSD_INDEXER_H

/* "Look for games": list the files, then work out what they are (reads them: CRCs, disc headers).
 * Only ever run because the user asked, like Console Mode: never by itself */
void indexer_identify(void);
void indexer_request(void);        /* list the files only (names, quick); also only on request */
int indexer_busy(void);
void indexer_stop(void);            /* stop Look for games (what's done stays; the next one carries on) */
typedef struct {
	int stage;                      /* 0 idle, 1 listing files, 2 identifying */
	int listed;                     /* files listed (stage 1) */
	int done, total;                /* identified this round, of how many (stage 2) */
	int pending;                    /* asked for and not finished (e.g. waiting for a game to close) */
	char system[64], file[160];     /* what it's on */
} indexer_progress_t;
void indexer_progress(indexer_progress_t *p);
int indexer_identifying(void);     /* working out what game files are */
int indexer_generation(void);      /* bumps after each finished scan */
void indexer_start(void);          /* background thread */
#include <sqlite3.h>
/* games whose drive came up under another /media/usb<n>: moved there in the library (how many drives) */
int indexer_relocate(sqlite3 *db);

#endif
