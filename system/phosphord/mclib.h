/* phosphord: the memory card library (one folder a card, one .mcd a version; the same as SS1 Tool's).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_MCLIB_H
#define PHOSD_MCLIB_H

#include <stddef.h>

const char *mclib_dir(void);                                   /* <fat>/phosphor/memcards */
int mclib_safe_name(const char *in, char *out, size_t n);       /* a usable card name; -1 if nothing's left */
/* the library card this image most likely is (0..1; 0.5 or more: the same card), its name in card */
double mclib_recognise(const unsigned char *img, char *card, size_t n);
int mclib_exists(const char *card);
/* a new version of card (made if needed); rel = "card/version.mcd"; *same: it matched the latest one */
int mclib_store(const char *card, const unsigned char *img, char *rel, size_t rn, int *same);
int mclib_valid(const char *rel);                               /* card/version.mcd or an old loose .mcd */
char *mclib_list(void);                                         /* rel\tbytes\tmtime lines, newest first */
void mclib_suggest(char *out, size_t n);                        /* "Memory card N", not used yet */

#endif
