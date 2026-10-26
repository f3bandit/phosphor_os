/* phosphord: the dock's NVMe bridge (JMicron JMS583, 152d:0583): its phantom drive, and its real drive
 * going missing.
 *
 * With its slot empty the bridge still shows a SCSI device: never ready, and it stalls the SCSI
 * shutdown, so it's taken offline and deleted. But a real drive can take a while to come up on a cold
 * start, and before it does it looks exactly the same: a device with no disk. The SS1's own Linux image
 * has a udev rule (/etc/udev/rules.d/60-jms583-phantom.rules) that deletes it after 15 seconds, which
 * takes a slow real drive away until the next reboot. And the SS1's kernel has no SCSI generic or bsg
 * driver, so nothing can ask the bridge whether a drive is in the slot until its disk shows up.
 *
 * So here:
 *  - the SS1's 15-second rule is turned off (renamed to .rules.off; uninstall.sh puts it back). A MiSTer
 *    Linux update may bring it back: it's checked every time phosphord starts.
 *  - a device on the bridge that still has no disk after a minute is the phantom, and is deleted.
 *  - the bridge with no device at all (deleted by that rule, by the minute here, or dropped): it's
 *    asked to look again, once. A real drive comes back (udev mounts it); an empty slot's phantom is
 *    deleted again a minute later, and that bridge isn't asked again.
 * Looks at /sys only, every 5 s (no USB traffic, except the one look again).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#include "phantom.h"
#include "log.h"
#include "paths.h"
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/klog.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static const char *sysfs(void) { const char *s = getenv("PHOS_SYS"); return s && *s ? s : "/sys"; }
static const char *etc_rules(void) { const char *s = getenv("PHOS_UDEV_RULES"); return s && *s ? s : "/etc/udev/rules.d"; }

static int read_str(const char *path, char *out, size_t n)
{
	FILE *f = fopen(path, "r");
	if (!f) return -1;
	if (!fgets(out, (int)n, f)) out[0] = 0;
	fclose(f);
	out[strcspn(out, "\r\n")] = 0;
	return 0;
}

static void write_path(const char *p, const char *v)
{
	FILE *f = fopen(p, "w");
	if (f) { fputs(v, f); fclose(f); }
}

static long long mono(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (long long)t.tv_sec;
}

/* the USB device above a sysfs path, when it's the JMS583: its "<path>#<devnum>" (a new number when it's
 * plugged in again) */
static int jms583_above(const char *path, char *usb, size_t un)
{
	char real[PATH_MAX];
	if (!realpath(path, real)) return 0;
	for (;;) {
		char v[16] = "", d[16] = "", n[16] = "", f[PATH_MAX + 16];
		snprintf(f, sizeof(f), "%s/idVendor", real);
		if (!read_str(f, v, sizeof(v))) {
			snprintf(f, sizeof(f), "%s/idProduct", real);
			read_str(f, d, sizeof(d));
			snprintf(f, sizeof(f), "%s/devnum", real);
			read_str(f, n, sizeof(n));
			const char *b = strrchr(real, '/');
			if (usb) snprintf(usb, un, "%.40s#%.8s", b ? b + 1 : real, n);
			return !strcmp(v, "152d") && !strcmp(d, "0583");
		}
		char *e = strrchr(real, '/');
		if (!e || e == real) return 0;
		*e = 0;
	}
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

/* SCSI devices ("h:c:t:l") under a host's device directory, any depth below targets */
static int host_has_device(const char *host)
{
	char p[PATH_MAX];
	snprintf(p, sizeof(p), "%s/class/scsi_device", sysfs());
	DIR *d = opendir(p);
	if (!d) return 0;
	struct dirent *e;
	int found = 0;
	size_t hl = strlen(host + 4);                 /* "host0" -> "0" */
	while ((e = readdir(d)))
		if (!strncmp(e->d_name, host + 4, hl) && e->d_name[hl] == ':') { found = 1; break; }
	closedir(d);
	return found;
}

/* ---------- the SS1's own 15-second rule: off ---------- */

#define SS1_RULE "60-jms583-phantom.rules"

static int root_ro(void)
{
	struct statvfs s;
	return !getenv("PHOS_UDEV_RULES") && statvfs("/", &s) == 0 && (s.f_flag & ST_RDONLY);
}

static void ss1_rule_off(void)
{
	char on[PATH_MAX], off[PATH_MAX + 8];
	snprintf(on, sizeof(on), "%s/%s", etc_rules(), SS1_RULE);
	snprintf(off, sizeof(off), "%s.off", on);
	if (access(on, F_OK)) return;
	int was_ro = root_ro();
	if (was_ro && mount(NULL, "/", NULL, MS_REMOUNT, NULL)) { plog("storage: couldn't turn the SS1's 15-second dock rule off (/ is read-only)"); return; }
	if (!access(off, F_OK)) unlink(off);           /* a Linux update brought it back: keep the newer one */
	if (rename(on, off)) plog("storage: couldn't turn the SS1's 15-second dock rule off");
	else {
		plog("storage: the SS1's 15-second dock rule is off (%s.off); phosphord waits a minute instead", SS1_RULE);
		sync();
		if (!getenv("PHOS_UDEV_RULES") && system("udevadm control --reload >/dev/null 2>&1") != 0) plog("storage: udevadm reload didn't run");
	}
	if (was_ro) mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);
}

/* ---------- the kernel's log of this boot, kept on the card ----------
 * The kernel keeps its log in memory, so a boot that went wrong (a drive that didn't show up) leaves
 * nothing once the SS1 is restarted. Saved once per boot, 90 s in (after the drives have come up), to
 * logs/boot-kernel.log; the boot before is kept as boot-kernel.prev.log. */
static void klog_save(void)
{
	static int done;
	if (done || getenv("PHOS_NO_KLOG")) return;
	struct timespec up;
	clock_gettime(CLOCK_BOOTTIME, &up);
	if (up.tv_sec < 90) return;
	done = 1;
	char id[64] = "", path[PATH_MAX], prev[PATH_MAX], first[128] = "";
	read_str("/proc/sys/kernel/random/boot_id", id, sizeof(id));
	snprintf(path, sizeof(path), "%s/phosphor/logs/boot-kernel.log", p_fat());
	snprintf(prev, sizeof(prev), "%s/phosphor/logs/boot-kernel.prev.log", p_fat());
	if (!read_str(path, first, sizeof(first)) && id[0] && strstr(first, id)) return;   /* this boot's, already (phosphord restarted) */
	int n = klogctl(10, NULL, 0);                    /* SYSLOG_ACTION_SIZE_BUFFER */
	if (n <= 0 || n > (4 << 20)) return;
	char *b = malloc((size_t)n);
	if (!b) return;
	int got = klogctl(3, b, n);                      /* SYSLOG_ACTION_READ_ALL: doesn't clear */
	if (got > 0) {
		rename(path, prev);
		FILE *f = fopen(path, "w");
		if (f) {
			time_t t = time(NULL);
			struct tm tm;
			char at[32];
			localtime_r(&t, &tm);
			strftime(at, sizeof(at), "%Y-%m-%d %H:%M:%S", &tm);
			fprintf(f, "boot %s, saved %s, %ld s after it started\n", id, at, (long)up.tv_sec);
			fwrite(b, 1, (size_t)got, f);
			fclose(f);
		}
	}
	free(b);
}

/* ---------- the tick ---------- */

#define MAXD 8
static struct { char name[32]; long long since; } bare[MAXD];      /* devices without a disk, since when */
static int nbare;
static char asked[MAXD][64];     /* bridges asked to look again (once each) */
static int nasked;
static char gave_up[MAXD][64];   /* bridges whose phantom was deleted: not asked again */
static int ngave_up;
static int removed, rescued;
static char watching[64];         /* a bridge asked to look again: say when its drive is back */

int phantom_removed(void) { return removed; }

static int in(char list[][64], int n, const char *s)
{
	for (int i = 0; i < n; i++) if (!strcmp(list[i], s)) return 1;
	return 0;
}

static void add(char list[][64], int *n, const char *s)
{
	if (in(list, *n, s)) return;
	if (*n < MAXD) snprintf(list[(*n)++], 64, "%s", s);
}

void phantom_tick(void)
{
	static long long last;
	static int started;
	long long now = mono();
	const char *ev = getenv("PHOS_PHANTOM_EVERY");          /* (tests) */
	const char *wv = getenv("PHOS_PHANTOM_WAIT");           /* (tests) */
	long long wait = wv ? atoll(wv) : 60;
	if (!started) { started = 1; ss1_rule_off(); }
	if (last && now - last < (ev ? atoi(ev) : 5)) return;
	klog_save();
	last = now;

	/* devices on the bridge: a disk, or how long without one */
	char p[PATH_MAX];
	snprintf(p, sizeof(p), "%s/class/scsi_device", sysfs());
	int nkeep = 0;
	__typeof__(bare) keep;
	DIR *d = opendir(p);
	struct dirent *e;
	while (d && (e = readdir(d))) {
		if (e->d_name[0] == '.' || strlen(e->d_name) >= sizeof(bare[0].name)) continue;
		char dev[PATH_MAX], usb[64];
		snprintf(dev, sizeof(dev), "%s/%s/device", p, e->d_name);
		if (!jms583_above(dev, usb, sizeof(usb))) continue;
		if (has_disk(e->d_name)) {
			if (*watching && !strcmp(watching, usb)) {
				rescued++;
				plog("storage: the dock's NVMe drive is back (%s)", e->d_name);
				watching[0] = 0;
			}
			continue;
		}
		long long since = now;
		for (int i = 0; i < nbare; i++) if (!strcmp(bare[i].name, e->d_name)) since = bare[i].since;
		if (now - since >= wait) {
			/* a minute and still no disk: the empty slot's phantom */
			char f[PATH_MAX + 16];
			snprintf(f, sizeof(f), "%s/state", dev);
			write_path(f, "offline");
			snprintf(f, sizeof(f), "%s/delete", dev);
			write_path(f, "1");
			removed++;
			if (in(asked, nasked, usb)) add(gave_up, &ngave_up, usb);   /* asked once already: an empty slot */
			if (!strcmp(watching, usb)) watching[0] = 0;
			plog("storage: removed the dock's empty-slot phantom drive (JMS583 SCSI %s, no disk after %llds)", e->d_name, now - since);
		} else if (nkeep < MAXD) {
			snprintf(keep[nkeep].name, sizeof(keep[0].name), "%s", e->d_name);
			keep[nkeep++].since = since;
		}
	}
	if (d) closedir(d);
	memcpy(bare, keep, sizeof(keep[0]) * (size_t)nkeep);
	nbare = nkeep;

	/* the bridge with no device at all: ask it to look again, once */
	snprintf(p, sizeof(p), "%s/class/scsi_host", sysfs());
	d = opendir(p);
	while (d && (e = readdir(d))) {
		if (strncmp(e->d_name, "host", 4)) continue;
		char dev[PATH_MAX], usb[64];
		snprintf(dev, sizeof(dev), "%s/%s/device", p, e->d_name);
		if (!jms583_above(dev, usb, sizeof(usb)) || host_has_device(e->d_name)) continue;
		if (in(asked, nasked, usb) || in(gave_up, ngave_up, usb)) continue;
		add(asked, &nasked, usb);
		char f[PATH_MAX + 16];
		snprintf(f, sizeof(f), "%s/%s/scan", p, e->d_name);
		/* the scan waits for the drive: not in phosphord's loop */
		pid_t c = fork();
		if (c == 0) {                               /* only open/write after fork: phosphord has threads */
			if (fork() == 0) { int fd = open(f, O_WRONLY); if (fd >= 0) { (void)!write(fd, "- - -", 5); close(fd); } }
			_exit(0);
		}
		if (c > 0) waitpid(c, NULL, 0);
		snprintf(watching, sizeof(watching), "%s", usb);
		plog("storage: the dock's NVMe drive was missing (JMS583 %s, nothing on it): asked the dock to look again", e->d_name);
	}
	if (d) closedir(d);
}
