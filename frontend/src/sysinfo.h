/* Phosphor frontend: live system state for the status bar.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_SYSINFO_H
#define PHOS_SYSINFO_H

typedef struct {
	int share;      /* a CIFS/NFS share is mounted */
	int ethernet;   /* wired link up */
	int wifi;       /* -1 = no WiFi adapter, 0 = not connected, 1..3 = signal bars */
	int bt;         /* -1 = no Bluetooth adapter, else connected devices */
} sysinfo_t;

void sysinfo_read(sysinfo_t *s);

/* mounts_changed: true once after /proc/self/mounts changes (a share or drive came or went) */
int mounts_changed(void);

#endif
