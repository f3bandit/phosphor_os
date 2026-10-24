/* Phosphor shell: client for phosphord, the system service.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_API_H
#define PHOS_API_H

#include "cJSON.h"

typedef enum { CAT_DISC, CAT_CARTRIDGE, CAT_HANDHELD, CAT_ARCADE, CAT_COMPUTER, CAT_COUNT } category_t;
const char *category_name(category_t c);

/* Send one request ({"cmd":...}); returns the reply (free with cJSON_Delete) or NULL if
 * phosphord can't be reached. Starts phosphord if it isn't running. */
cJSON *api_call(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* the same for one slow request (up to ms), never retried */
void api_reset(void);   /* a new connection for the next request */
cJSON *api_call_slow(int ms, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* reply helpers */
int api_ok(const cJSON *r);
const char *api_str(const cJSON *o, const char *key);      /* NULL if missing or null */
double api_num(const cJSON *o, const char *key, double def);

/* send one command to MiSTer Main (/dev/MiSTer_cmd) directly (framebuffer setup) */
int mister_cmd(const char *cmd, char *err, unsigned long errn);

#endif
