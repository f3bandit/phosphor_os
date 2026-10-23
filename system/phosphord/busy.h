/* phosphord: markers for heavy work, for the health-watch module's power check.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * <fat>/phosphor/state/busy/<name> exists while heavy work runs (not box art: it can run for hours
 * unattended, and switching a MiSTer off at the plug is normal) (boot id, start time, what). If the
 * power goes, it's still there at the next start-up, and health-watch counts it. Synced to the card
 * when written, so a sudden power cut can't lose it. */
#ifndef PHOSD_BUSY_H
#define PHOSD_BUSY_H

/* at start-up: markers this boot left (phosphord was restarted mid-job) are stale: remove them */
void busy_init(void);
void busy_mark(const char *name, const char *what);
void busy_clear(const char *name);

#endif
