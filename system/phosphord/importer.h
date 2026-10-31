/* phosphord: one-time import of artwork and details other tools left on the card.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_IMPORTER_H
#define PHOSD_IMPORTER_H

#include "sqlite3.h"
#include <stddef.h>

typedef struct { int games, details, art; } import_result_t;

/* Reads gamelist.xml (EmulationStation format) and media/<title>.png|jpg|txt (Console Mode's
 * layout) in each system's games folders, fills empty fields only, and copies art into Phosphor's
 * art store. Run on request; after that Phosphor reads only its own library. */
import_result_t import_existing(sqlite3 *db);

/* gamelist.xml helpers (src_local.c too): a tag's text between from and to (malloc'd, unescaped,
 * trimmed; NULL if missing or empty); a path in it made absolute against the list's folder */
char *gl_tag_text(const char *from, const char *to, const char *tag);
void gl_resolve(const char *dir, const char *rel, char *out, size_t n);

#endif
