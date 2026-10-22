/* phosphord: MiSTer's own file sharing (Samba) on and off from the menu.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_SAMBA_H
#define PHOSD_SAMBA_H

#include <stddef.h>

typedef struct {
	int available;   /* MiSTer's Samba is there (linux/_samba.sh or samba.sh, S91smb) */
	int on;          /* starts with the system (linux/samba.sh) */
	int running;     /* smbd is running now */
	int busy, want;  /* turning it on (want 1) or off (want 0) right now */
	int ok;          /* the last change worked; error says why not */
	char error[160];
} samba_state_t;

void samba_state(samba_state_t *s);
int samba_start(int on);   /* in the background (starting smbd takes seconds); 1 if already busy */

#endif
