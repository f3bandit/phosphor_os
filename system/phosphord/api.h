/* phosphord: the API, one JSON request in, one JSON reply out.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_API_H
#define PHOSD_API_H

#include "sqlite3.h"

/* returns a malloc'd JSON reply (one line, no newline) */
char *api_handle(sqlite3 *db, const char *request);

#endif
