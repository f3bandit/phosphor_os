/* Phosphor shell: the link to phosphor-main (Phosphor's fork of MiSTer Main), when it's installed.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_MAINLINK_H
#define PHOS_MAINLINK_H

#include <stddef.h>

/* connect and take the screen; 1 when the Main fork answered (0: stock Main, use F9) */
int mainlink_start(void);
/* call often: reads events, reconnects (and takes the screen again) if Main restarted.
 * Returns 1 when it just (re)took the screen, so the caller can set its video mode again. */
int mainlink_poll(void);
int mainlink_connected(void);
int mainlink_fd(void);              /* to wait on (events from Main); -1 if not connected */
/* "MiSTer menu": give the screen to Main's own menu; the shell then waits (input ignored) until
 * Launch Phosphor in Main's OSD asks for it again. 0 when there's no Main fork to give it to */
int mainlink_give(void);
int mainlink_hidden(void);
/* Main sends the controllers' buttons (MiSTer's mapping and repeat); the shell then doesn't read them itself */
int mainlink_pads(void);
/* run a script the way MiSTer's Scripts menu does (MiSTer's terminal; the shell steps aside): 0 started.
 * mainlink_poll returns 1 when it's over and the screen is the shell's again; then
 * mainlink_script_result() gives its exit status once (-1 if nothing finished) */
int mainlink_script(const char *path);
int mainlink_script_running(void);
int mainlink_script_result(void);

/* MiSTer's controller mapping, run by Main for the shell (phosphor-main api 4): what is "start VVVV:PPPP",
 * "skip" or "cancel"; 0 sent. mainlink_map_state: 1 while it runs; step is MiSTer's (-100 before the
 * first), done/saved once it's over, err when Main refused */
int mainlink_can_map(void);
int mainlink_map(const char *what);
int mainlink_map_state(int *step, int *done, int *saved, char *dev, size_t dn, char *err, size_t en);

#endif
