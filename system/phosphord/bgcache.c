/* phosphord: slow answers kept and refreshed in the background (bgcache.h).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#include "bgcache.h"
#include <stdlib.h>
#include <string.h>

static void keep(bgcache_t *c, char *v)
{
	pthread_mutex_lock(&c->lk);
	free(c->val);
	c->val = v;
	c->have = 1;
	c->at = time(NULL);
	c->running = 0;
	pthread_mutex_unlock(&c->lk);
}

static void *worker(void *arg)
{
	bgcache_t *c = arg;
	keep(c, c->fn(c->arg));
	return NULL;
}

char *bgcache_now(bgcache_t *c)
{
	char *v = c->fn(c->arg);
	keep(c, v ? strdup(v) : NULL);
	return v;
}

char *bgcache_get(bgcache_t *c, int wait)
{
	pthread_mutex_lock(&c->lk);
	int have = c->have;
	if (!have && wait && !c->running) {
		pthread_mutex_unlock(&c->lk);
		return bgcache_now(c);
	}
	if (!c->running && (!have || time(NULL) - c->at >= c->ttl)) {
		pthread_t t;
		pthread_attr_t a;
		pthread_attr_init(&a);
		pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
		if (pthread_create(&t, &a, worker, c) == 0) c->running = 1;
		pthread_attr_destroy(&a);
	}
	char *v = c->val ? strdup(c->val) : NULL;
	pthread_mutex_unlock(&c->lk);
	return v;
}

void bgcache_stale(bgcache_t *c)
{
	pthread_mutex_lock(&c->lk);
	c->at = 0;
	pthread_mutex_unlock(&c->lk);
}
