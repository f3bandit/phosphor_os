/* phosphord: the dock's NVMe bridge, its phantom drive and its real drive going missing (phantom.c).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_PHANTOM_H
#define PHOSD_PHANTOM_H

void phantom_tick(void);     /* the main loop, every 2 s: looks every 5 s */
int phantom_removed(void);   /* how many were removed since phosphord started */

#endif
