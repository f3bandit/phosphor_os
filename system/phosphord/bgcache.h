/* phosphord: answers that take seconds to work out (a module's drive list, the update packages, cloud
 * status) kept and refreshed in the background, so phosphord (one request at a time) never makes the
 * TV or the app wait for them.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_BGCACHE_H
#define PHOSD_BGCACHE_H
#include <pthread.h>
#include <time.h>

typedef struct bgcache {
	pthread_mutex_t lk;
	char *val;                 /* the last answer (NULL: none yet) */
	int have;                  /* an answer was worked out (val may still be NULL) */
	time_t at;
	int running, ttl;
	char *(*fn)(void *);       /* works it out (malloc'd, or NULL), in a background thread */
	void *arg;
} bgcache_t;

#define BGCACHE(f, a, t) { PTHREAD_MUTEX_INITIALIZER, NULL, 0, 0, 0, (t), (f), (a) }

/* a copy of the last answer (caller frees; NULL when none). Older than ttl: refreshed in the background.
   wait: with no answer yet, wait for it up to 1.2 s (then NULL: the next get has it) */
char *bgcache_get(bgcache_t *c, int wait);
/* the answer is out of date (a drive mounted, a package arrived): refreshed on the next get */
void bgcache_stale(bgcache_t *c);
/* work it out now and keep it (for an action that has to see the newest) */
char *bgcache_now(bgcache_t *c);
#endif
