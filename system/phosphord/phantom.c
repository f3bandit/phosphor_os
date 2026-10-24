/* phosphord: the JMicron phantom-drive rule. The SS1 dock's NVMe bridge (JMicron JMS583, 152d:0583)
 * shows a SCSI device even when its slot is empty: never ready, and it stalls the SCSI shutdown. The
 * SS1's own Linux image removes it with a udev rule in /etc (60-jms583-phantom.rules), but a MiSTer
 * Linux update replaces /etc, and stock MiSTer never had it. So phosphord does the same itself: a JMS583
 * SCSI device that still has no disk on two looks 15 s apart is taken offline and deleted. A slot with a
 * drive gets its disk within a second or two and is never touched. Looks at /sys only (no USB traffic).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#include "phantom.h"
#include "log.h"
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const char *sysfs(void) { const char *s = getenv("PHOS_SYS"); return s && *s ? s : "/sys"; }

static int read_str(const char *path, char *out, size_t n)
{
	FILE *f = fopen(path, "r");
	if (!f) return -1;
	if (!fgets(out, (int)n, f)) out[0] = 0;
	fclose(f);
	out[strcspn(out, "\r\n")] = 0;
	return 0;
}

/* the USB device above a SCSI device is the JMS583 bridge */
static int on_jms583(const char *scsi)
{
	char p[PATH_MAX], real[PATH_MAX];
	snprintf(p, sizeof(p), "%s/class/scsi_device/%s/device", sysfs(), scsi);
	if (!realpath(p, real)) return 0;
	for (char *e = real + strlen(real); e > real; ) {
		char v[16] = "", d[16] = "", f[PATH_MAX + 16];
		snprintf(f, sizeof(f), "%s/idVendor", real);
		if (!read_str(f, v, sizeof(v))) {
			snprintf(f, sizeof(f), "%s/idProduct", real);
			read_str(f, d, sizeof(d));
			return !strcmp(v, "152d") && !strcmp(d, "0583");
		}
		e = strrchr(real, '/');
		if (!e || e == real) break;
		*e = 0;
	}
	return 0;
}

static int has_disk(const char *scsi)
{
	char p[PATH_MAX];
	snprintf(p, sizeof(p), "%s/class/scsi_device/%s/device/block", sysfs(), scsi);
	DIR *d = opendir(p);
	if (!d) return 0;
	struct dirent *e;
	int n = 0;
	while ((e = readdir(d))) if (e->d_name[0] != '.') n++;
	closedir(d);
	return n > 0;
}

static void write_str(const char *scsi, const char *file, const char *v)
{
	char p[PATH_MAX];
	snprintf(p, sizeof(p), "%s/class/scsi_device/%s/device/%s", sysfs(), scsi, file);
	FILE *f = fopen(p, "w");
	if (f) { fputs(v, f); fclose(f); }
}

static char seen[8][32];        /* JMS583 devices without a disk last time */
static int nseen;
static int removed;

int phantom_removed(void) { return removed; }

void phantom_tick(void)
{
	static time_t last;
	time_t now = time(NULL);
	const char *ev = getenv("PHOS_PHANTOM_EVERY");          /* (tests) */
	if (now - last < (ev ? atoi(ev) : 15)) return;
	last = now;
	char p[PATH_MAX], cur[8][32];
	int ncur = 0;
	snprintf(p, sizeof(p), "%s/class/scsi_device", sysfs());
	DIR *d = opendir(p);
	if (!d) return;
	struct dirent *e;
	while ((e = readdir(d)) && ncur < 8) {
		if (e->d_name[0] == '.' || strlen(e->d_name) >= sizeof(cur[0])) continue;
		if (has_disk(e->d_name) || !on_jms583(e->d_name)) continue;
		int before = 0;
		for (int i = 0; i < nseen; i++) if (!strcmp(seen[i], e->d_name)) before = 1;
		if (before) {
			/* empty on two looks: the phantom */
			write_str(e->d_name, "state", "offline");
			write_str(e->d_name, "delete", "1");
			removed++;
			plog("storage: removed the dock's empty-slot phantom drive (JMS583 SCSI %s)", e->d_name);
		} else snprintf(cur[ncur++], sizeof(cur[0]), "%.31s", e->d_name);
	}
	closedir(d);
	memcpy(seen, cur, sizeof(cur));
	nseen = ncur;
}
