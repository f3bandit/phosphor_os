/* phosphord: the systems Phosphor knows, and how each one is started.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_SYSTEMS_H
#define PHOSD_SYSTEMS_H

#include <stddef.h>

typedef enum { CAT_DISC, CAT_CARTRIDGE, CAT_HANDHELD, CAT_ARCADE, CAT_COMPUTER, CAT_COUNT } category_t;

typedef struct {
	char id[24];          /* stable id, also the systems.ini section */
	char name[48];
	category_t cat;
	char folders[96];     /* games folder names, | separated */
	char rbf[96];         /* core(s) relative to the SD root, no date, no .rbf; | separated */
	char exts[96];        /* | separated, lower case */
	char type;            /* MGL file type: f (file) or s (storage/disc) */
	int index;            /* MGL file slot */
	int delay;            /* MGL delay in seconds */
	char cfgname[32];     /* core name MiSTer Main uses for config files, if not the core's file name */
} gsys_t;

extern gsys_t *SYS;
extern int NSYS;

void systems_load(void);                         /* built-ins, then config/systems.ini */
const gsys_t *system_by_id(const char *id);
const char *category_name(category_t c);
int system_dirs(const gsys_t *s, char out[][512], int max);   /* existing games folders */
int system_core(const gsys_t *s, char *rbf, size_t n);        /* installed core, 1 if found */
int has_ext(const char *name, const char *exts);

#endif
