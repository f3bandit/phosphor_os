/* phosphord: box art and details for identified games.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_SCRAPER_H
#define PHOSD_SCRAPER_H

#include "sqlite3.h"
#include "cJSON.h"

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

/* this run, for the Getting box art screen */
typedef struct {
	int running;
	int total, done, found, not_found, later;   /* games: at the start of the run, looked at, ... */
	char system[48], game[120], source[40];     /* what it's on now (source: the one being asked) */
	char status[160];
	char note[200];     /* a source that stopped for this run, and why ("" when none) */
} scrape_progress_t;
void scrape_note(const char *t);          /* sets this run's note (the first one wins) */
void scraper_progress(scrape_progress_t *out);

void scraper_start(void);                  /* background thread */
void scraper_set(sqlite3 *db, int on);     /* on: go through the games once now (only ever on request); off: stop */
void scraper_kick(void);                   /* run once now (the user asked) */
void scraper_info(sqlite3 *db, scrape_info_t *out);
int scraper_retry(sqlite3 *db, long long game_id);   /* look this game up again */
int scraper_sources(scrape_source_t *out, int max);   /* in the user's order */
/* save the order and which are off (comma separated ids); games missing art or a description are
 * looked at again */
int scraper_set_sources(sqlite3 *db, const char *order, const char *off);

/* account keys in scraper.ini (ScreenScraper, TheGamesDB, IGDB): set one, or list what's set */
typedef struct { char sec[16], key[16], value[128]; int set; } scrape_account_t;
#define SCRAPE_ACCOUNTS 7
int scraper_account_ok(const char *sec, const char *key);
int scraper_set_account(const char *sec, const char *key, const char *val);
void scraper_accounts(scrape_account_t *out, int *n);

/* the Phosphor app doing the lookups on the PC: the games to do and the settings; one game's result
   (NULL ok, else the error); the app finished */
cJSON *scraper_todo(sqlite3 *db, cJSON *req);
const char *scraper_result(sqlite3 *db, cJSON *req);
void scraper_pc_done(const char *status);
const char *scraper_undo(sqlite3 *db, cJSON *req);

#endif
