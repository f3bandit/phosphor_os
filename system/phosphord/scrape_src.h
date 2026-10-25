/* phosphord: scraper sources (libretro, ArcadeDB, LaunchBox, TheGamesDB, ScreenScraper, IGDB).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Every source answers the same question: "this game (by content), what do you have?" Sources are
 * asked in the user's order; for each field the first answer wins, and fields the game already
 * has are never replaced. */
#ifndef PHOSD_SCRAPE_SRC_H
#define PHOSD_SCRAPE_SRC_H

#include "sqlite3.h"
#include <stddef.h>

enum { F_DESC, F_DEV, F_PUB, F_GENRE, F_YEAR, F_PLAYERS, F_REGION, F_COUNT };
#define MAX_ART 8

typedef struct {
	long long id;
	char sys[32];          /* Phosphor system id */
	char key[200];         /* identity key */
	char crc[16];          /* cartridges */
	char serial[64];       /* discs: product code; arcade: MAME set name */
	long size;             /* discs: data track size (0 if unknown) */
	char title[512];       /* file name: matched only exactly (see src_libretro) */
	char official[400];    /* official No-Intro/Redump name, once a source matched by content */
	char lrsys[100];       /* libretro system the official name came from */
	int disc, arcade;
} sgame_t;

typedef struct {
	char *f[F_COUNT];      /* malloc'd or NULL */
	char *art[MAX_ART];    /* box art URLs, best first */
	char art_src[MAX_ART][16];
	int nart;
	char found_by[160];    /* "libretro, LaunchBox" */
} sres_t;

void sres_set(sres_t *r, int field, const char *v);        /* only if empty; trims; ignores "" */
void sres_art(sres_t *r, const char *src, const char *url);
void sres_free(sres_t *r);
int sres_complete(const sres_t *r, int has_art);          /* nothing more to ask for */

/* lookup: 1 found something, 0 not known there, -1 network trouble (try again later),
 * -2 not set up (account or key missing) */
typedef struct {
	const char *id, *name;
	const char *needs;                    /* NULL, or what has to be set in scraper.ini */
	int (*ready)(void);                   /* set up (keys present) */
	int (*lookup)(sqlite3 *db, sgame_t *g, sres_t *r);
} source_t;

extern const source_t SRC_LIBRETRO, SRC_ARCADEDB, SRC_LAUNCHBOX, SRC_TGDB, SRC_SCREENSCRAPER, SRC_IGDB;
/* fan-made box art (src_fanart.c): asked first, always; not in the user's order */
extern const source_t SRC_FANART;
int fanart_pack_get(const char *name, const char *dest);   /* "pack:<name>" art: 0 done, 1 not there, -1 network trouble */

/* settings from config/scraper.ini ("" when not set) */
const char *scfg(const char *section, const char *key);

/* shared helpers */
void scrape_status(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void name_base(const char *official, char *out, size_t n);   /* "Final Fantasy III (USA)" -> "Final Fantasy III" */
void xml_unescape(char *s);

#endif
