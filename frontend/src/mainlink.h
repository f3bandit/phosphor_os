/* Phosphor shell: the link to phosphor-main (Phosphor's fork of MiSTer Main), when it's installed.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_MAINLINK_H
#define PHOS_MAINLINK_H

/* connect and take the screen; 1 when the Main fork answered (0: stock Main, use F9) */
int mainlink_start(void);
/* call often: reads events, reconnects (and takes the screen again) if Main restarted.
 * Returns 1 when it just (re)took the screen, so the caller can set its video mode again. */
int mainlink_poll(void);
int mainlink_connected(void);

#endif
