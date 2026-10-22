/* phosphord: finds games on the SD card, USB drives and the network share.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_INDEXER_H
#define PHOSD_INDEXER_H

void indexer_request(void);        /* run (again) soon; safe to call any time */
int indexer_busy(void);
int indexer_identifying(void);     /* working out what game files are */
int indexer_generation(void);      /* bumps after each finished scan */
void indexer_start(void);          /* background thread */

#endif
