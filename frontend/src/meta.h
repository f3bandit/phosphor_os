/* Phosphor frontend: game details (box art, description, year, publisher...).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_META_H
#define PHOS_META_H

typedef struct {
	char *name, *desc, *image, *date, *developer, *publisher, *genre, *players;
} meta_t;

/* Load what's known about one system's games: its Phosphor library folder, plus the
 * gamelist.xml files in its games folders. */
void meta_load(const char *system_id, char dirs[][512], int ndirs);
void meta_free(void);

/* Details for one game (never NULL; fields may be NULL). image is an absolute path or NULL. */
const meta_t *meta_for(const char *game_path);

#endif
