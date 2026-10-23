/* phosphord: box art and details for identified games.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_SCRAPER_H
#define PHOSD_SCRAPER_H

#include "sqlite3.h"

typedef struct {
	int on;            /* looking things up right now (asked for) */
	int running;       /* working right now */
	int pending;       /* identified games not looked up yet */
	int done;          /* found */
	int missing;       /* looked up, not in the lists: link these with "Based on" */
	char status[160];  /* what it's doing, in words */
} scrape_info_t;

typedef struct {
	const char *id, *name, *needs;   /* needs: NULL, or what to set in config/scraper.ini */
	int on, ready;                   /* turned on; set up (has its key) */
} scrape_source_t;

void scraper_start(void);                  /* background thread */
void scraper_set(sqlite3 *db, int on);     /* on: go through the games once now (only ever on request); off: stop */
void scraper_kick(void);                   /* run once now (the user asked) */
void scraper_info(sqlite3 *db, scrape_info_t *out);
int scraper_retry(sqlite3 *db, long long game_id);   /* look this game up again */
int scraper_sources(scrape_source_t *out, int max);   /* in the user's order */
/* save the order and which are off (comma separated ids); games missing art or a description are
 * looked at again */
int scraper_set_sources(sqlite3 *db, const char *order, const char *off);

#endif
