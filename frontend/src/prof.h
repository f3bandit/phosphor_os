/* Phosphor's drawing profiler (a test build only: cmake -DPHOS_PROFILE=ON): LVGL's profiler hooks add up the
   time spent in each of its functions (self time, nested calls taken out) and how often; the shell writes the
   table to <tmp>/prof when it gets SIGUSR1 (kill -USR1 $(pidof phosphor)), and SIGUSR2 starts again from 0.
   Copyright (C) 2026 f3bandit. All rights reserved; licensed to Retro Remake (see LICENSE.md). */
#ifndef PHOS_PROF_H
#define PHOS_PROF_H
#include <stdint.h>
void prof_begin(const char *tag);
void prof_end(const char *tag);
void prof_px(const char *tag, uint64_t px);   /* pixels a blend covered, by kind */
void prof_poll(void);          /* from the main loop: writes / clears when a signal asked */
#define LV_PROFILER_BEGIN prof_begin(__func__)
#define LV_PROFILER_END prof_end(__func__)
#define LV_PROFILER_BEGIN_TAG(tag) prof_begin(tag)
#define LV_PROFILER_END_TAG(tag) prof_end(tag)
#endif
