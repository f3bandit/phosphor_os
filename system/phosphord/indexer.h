/* phosphord: finds games on the SD card, USB drives and the network share.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_INDEXER_H
#define PHOSD_INDEXER_H

/* "Look for games": list the files, then work out what they are (reads them: CRCs, disc headers).
 * Only ever run because the user asked, like Console Mode: never by itself */
void indexer_identify(void);
void indexer_request(void);        /* list the files only (names, quick); also only on request */
int indexer_busy(void);
int indexer_identifying(void);     /* working out what game files are */
int indexer_generation(void);      /* bumps after each finished scan */
void indexer_start(void);          /* background thread */

#endif
