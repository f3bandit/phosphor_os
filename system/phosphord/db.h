/* phosphord: the library database.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_DB_H
#define PHOSD_DB_H

#include "sqlite3.h"

sqlite3 *db_open(void);              /* opens <data>/library.db, creating or upgrading the schema */
int db_exec(sqlite3 *db, const char *sql);
const char *db_meta_get(sqlite3 *db, const char *k, char *buf, int n);
void db_meta_set(sqlite3 *db, const char *k, const char *v);
int natcmp(const char *a, const char *b);
void game_key(const char *title, char *key, int n);   /* identity until a file is hashed */

#endif
