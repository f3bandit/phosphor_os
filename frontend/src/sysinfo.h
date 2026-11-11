/* Phosphor frontend: live system state for the status bar.
 * Copyright (C) 2026 f3bandit. All rights reserved; licensed to Retro Remake (see LICENSE.md). */
#ifndef PHOS_SYSINFO_H
#define PHOS_SYSINFO_H

typedef struct {
	int share;      /* a CIFS/NFS share is mounted */
	int ethernet;   /* wired link up */
	int wifi;       /* -1 = no WiFi adapter, 0 = not connected, 1..3 = signal bars */
	int bt;         /* -1 = no Bluetooth adapter, else connected devices */
	int dockir;     /* -1 = no dock IR receiver, 0 = switched off (Settings > Dock IR remote), 1 = on */
} sysinfo_t;

void sysinfo_read(sysinfo_t *s);

#endif
