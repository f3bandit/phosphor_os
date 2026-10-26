/* phosphord: screenshots (shot.c).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_SHOT_H
#define PHOSD_SHOT_H

#include "cJSON.h"
#include <stddef.h>

/* take one: in a game MiSTer's Main does ("game"), on Phosphor's screens the framebuffer is saved here
 * ("phosphor", file is set). 0 on success. */
int shot_take(char *where, size_t wn, char *file, size_t fn, char *err, size_t errn);
cJSON *shot_list(void);                                  /* {shots:[{id,core,size,at}], total, bytes}, newest first */
int shot_path(const char *id, char *out, size_t n);      /* id "<core>/<file>" -> its path; -1: not one */
int shot_thumb(const char *id, char *out, size_t n);     /* a small JPEG of it (made once) */
int shot_image(const char *id, int max, char *out, size_t n);   /* a JPEG of it, max pixels on its longest side (made once) */
int shot_delete(const char *id, char *err, size_t errn);

#endif
