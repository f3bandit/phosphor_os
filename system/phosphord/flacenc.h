/* phosphord: a small FLAC encoder for ripping music CDs (flacenc.c).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_FLACENC_H
#define PHOSD_FLACENC_H

#include <stdint.h>

typedef struct flacenc flacenc_t;
/* 44.1 kHz 16-bit stereo; tags: NULL-ended "KEY=value" (TITLE, ARTIST, ALBUM, TRACKNUMBER, DATE) */
flacenc_t *flacenc_open(const char *path, const char *const *tags);
int flacenc_write(flacenc_t *e, const int16_t *stereo, int frames);
int flacenc_close(flacenc_t *e);

#endif
