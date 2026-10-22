/* phosphord: box art and details for identified games.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_SCRAPER_H
#define PHOSD_SCRAPER_H

#include "sqlite3.h"

typedef struct {
	int on;            /* looking things up when the menu is up */
	int running;       /* working right now */
	int pending;       /* identified games not looked up yet */
	int done;          /* found */
	int missing;       /* looked up, not in the lists: link these with "Based on" */
	char status[160];  /* what it's doing, in words */
} scrape_info_t;

void scraper_start(void);                  /* background thread */
void scraper_set(sqlite3 *db, int on);     /* turn looking up on or off (kept in the library) */
void scraper_kick(void);                   /* new games may be waiting */
void scraper_info(sqlite3 *db, scrape_info_t *out);
int scraper_retry(sqlite3 *db, long long game_id);   /* look this game up again */

#endif
