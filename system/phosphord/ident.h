/* phosphord: what a game file is (CRC32 of a cartridge, serial of a disc, set name of an arcade game).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_IDENT_H
#define PHOSD_IDENT_H

#include "sqlite3.h"
#include "systems.h"

typedef struct {
	char crc32[9];       /* 8 hex digits, ROM data without copier headers; "" if not hashed */
	char serial[32];     /* disc product code (SLUS-00594, T-12705H, GM T-...) or arcade set name, for lookups */
	char key[80];        /* game identity: crc:xxxxxxxx, disc:<code>:<size>:<crc of 1st MB>, chd:<sha1>, mra:<set>; "" if none */
} ident_t;

/* Identify one file. Returns 0 if it could be read (key may still be empty: e.g. a .chd). */
int ident_file(const gsys_t *sys, const char *path, ident_t *out);

/* Identify files not done yet and give their games an identity (merging copies of the same game
 * found on several drives). Stops early when should_stop() says so. Returns games changed. */
/* while ident_pending runs: identified so far, of how many, and the system and file it's on */
void ident_progress(int *done, int *total, char *sys, size_t sn, char *file, size_t fn);
int ident_pending(sqlite3 *db, int (*should_stop)(void));

/* Title tidied for comparing: lower case, no (tags) or [tags], no hack/patch/translation words */
void title_norm(const char *title, char *out, int n);
/* 0..1000: how alike two tidied titles are */
int title_similarity(const char *a, const char *b);

#endif
