/* Phosphor's drawing profiler: see prof.h. Copyright (C) 2026 f3bandit. All rights reserved; licensed to Retro Remake. */
#include "prof.h"
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define NT 512
#define DEPTH 64
static struct { const char *tag; uint64_t self, total, n, px; } t[NT];
static struct { int i; uint64_t start, child; } st[DEPTH];
static int sp;
static volatile sig_atomic_t want_dump, want_clear;
static pthread_t main_thread;
static int have_main;
/* the main thread only: LVGL's drawing threads (LV_USE_OS) would tangle the stack; their time shows in the main
   thread's wait for them (refr_invalid_areas and the like) */
static int other_thread(void) { if (!have_main) { main_thread = pthread_self(); have_main = 1; } return !pthread_equal(pthread_self(), main_thread); }

static uint64_t now_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static int slot(const char *tag)
{
	unsigned h = (unsigned)(uintptr_t)tag * 2654435761u % NT;
	for (int k = 0; k < NT; k++, h = (h + 1) % NT) {
		if (t[h].tag == tag) return (int)h;
		if (!t[h].tag) { t[h].tag = tag; return (int)h; }
	}
	return -1;
}

void prof_px(const char *tag, uint64_t px) { if (other_thread()) return; int i = slot(tag); if (i >= 0) t[i].px += px; }

void prof_begin(const char *tag)
{
	if (other_thread()) return;
	if (sp >= DEPTH) { sp++; return; }
	st[sp].i = slot(tag);
	st[sp].child = 0;
	st[sp].start = now_ns();
	sp++;
}

void prof_end(const char *tag)
{
	(void)tag;
	if (other_thread()) return;
	if (sp <= 0) return;
	sp--;
	if (sp >= DEPTH) return;
	uint64_t d = now_ns() - st[sp].start;
	int i = st[sp].i;
	if (i >= 0) { t[i].total += d; t[i].self += d - st[sp].child; t[i].n++; }
	if (sp > 0 && sp - 1 < DEPTH) st[sp - 1].child += d;
}

static void on_usr1(int s) { (void)s; want_dump = 1; }
static void on_usr2(int s) { (void)s; want_clear = 1; }

static int cmp(const void *a, const void *b)
{
	const typeof(t[0]) *x = a, *y = b;
	return x->self < y->self ? 1 : x->self > y->self ? -1 : 0;
}

void prof_poll(void)
{
	static int init;
	if (!init) { init = 1; signal(SIGUSR1, on_usr1); signal(SIGUSR2, on_usr2); }
	if (want_clear) { want_clear = 0; for (int i = 0; i < NT; i++) t[i].self = t[i].total = t[i].n = t[i].px = 0; }
	if (!want_dump) return;
	want_dump = 0;
	static typeof(t[0]) c[NT];
	memcpy(c, t, sizeof(c));
	qsort(c, NT, sizeof(c[0]), cmp);
	const char *tp = getenv("PHOS_TMP");
	char p[300];
	snprintf(p, sizeof(p), "%s/prof", tp && *tp ? tp : "/tmp/phosphor");
	FILE *f = fopen(p, "w");
	if (!f) return;
	fprintf(f, "%10s %10s %9s %8s %7s  %s\n", "self ms", "total ms", "calls", "Mpx", "ns/px", "function");
	for (int i = 0; i < NT && c[i].tag; i++)
		if (c[i].n) fprintf(f, "%10.1f %10.1f %9llu %8.2f %7.2f  %s\n", c[i].self / 1e6, c[i].total / 1e6, (unsigned long long)c[i].n, c[i].px / 1e6, c[i].px ? (double)c[i].total / c[i].px : 0.0, c[i].tag);
	fclose(f);
}
