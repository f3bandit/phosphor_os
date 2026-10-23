/* phosphord: the health-watch module's power check (heavy work cut short with no trace = the power went).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_POWER_H
#define PHOSD_POWER_H

/* health-watch.sh power-state (after power-ack when ack): key=value lines, malloc'd (NULL: not installed) */
char *power_state(int ack);

#endif
