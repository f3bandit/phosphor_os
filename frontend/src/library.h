/* Phosphor frontend: game library (systems, folders, scanning) and launching.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_LIBRARY_H
#define PHOS_LIBRARY_H

#include <stddef.h>

typedef enum { CAT_DISC, CAT_CARTRIDGE, CAT_HANDHELD, CAT_ARCADE, CAT_COMPUTER, CAT_COUNT } category_t;

typedef struct {
	char id[24];          /* systems.ini section */
	char name[48];        /* shown to the user */
	category_t cat;
	char folders[96];     /* games folder names, | separated (first match per root wins) */
	char rbf[96];         /* core(s) relative to /media/fat without date or .rbf, | separated */
	char exts[96];        /* file extensions, | separated, lower case */
	char type;            /* MGL file type: 'f' (cartridge/file) or 's' (disc/storage) */
	int index;            /* MGL file slot */
	int delay;            /* MGL delay (seconds) */
	int count;            /* games found (-1 = not scanned yet) */
	int core_found;
} game_system_t;

typedef struct {
	char *path;           /* absolute */
	char *title;          /* file name without extension */
} game_t;

extern game_system_t *SYSTEMS;
extern int NSYSTEMS;

void library_init(void);                 /* built-in systems, then systems.ini overrides */
void library_scan_start(void);           /* count games in the background */
int library_scan_generation(void);       /* bumps when counts change */
int library_category_count(category_t c);/* games in a category */
int library_category_systems(category_t c, int *out, int max);
const char *category_name(category_t c);

/* the folders a system's games are in (games/<System> on each drive, or _Arcade) */
int library_system_dirs(const game_system_t *s, char out[][512], int max);

/* games of one system, sorted by title; caller frees with library_free_games */
game_t *library_games(const game_system_t *s, int *n);
void library_free_games(game_t *g, int n);

/* send one command to MiSTer Main (/dev/MiSTer_cmd); returns 0 on success, else fills err */
int mister_cmd(const char *cmd, char *err, size_t errn);

/* launch: returns 0 on success, else fills err */
int library_launch(const game_system_t *s, const game_t *g, char *err, size_t errn);

#endif
