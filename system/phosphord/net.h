/* phosphord: HTTP through curl (on MiSTer's Linux), with its certificate bundle.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_NET_H
#define PHOSD_NET_H

#include <stddef.h>

/* Results: 0 = got it (HTTP 200), 1 = not there (404), 2 = refused (401/403: login or key wrong),
 * -1 = no network or another failure. */

/* GET url into the file dest (written to dest.part first) */
int net_get_file(const char *url, const char *dest);

/* any request; the body (if any) is sent as-is; the reply is returned in *out (malloc'd, 0-ended,
 * caller frees). headers: NULL-ended list of "Name: value". http receives the status code. */
int net_request(const char *method, const char *url, const char *const *headers, const char *body,
                char **out, size_t *outlen, int *http);

void net_escape(const char *in, char *out, size_t n);   /* URL query/path escaping */

/* this thread's requests are cut off (curl stopped, -1 returned) as soon as *flag is set; NULL: never.
 * The scraper's Stop works within a moment this way, not after a slow source has answered. */
void net_set_cancel(volatile int *flag);

#endif
