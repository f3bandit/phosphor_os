/* phosphord: markers for heavy work, for the health-watch module's power check.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#include "busy.h"
#include "paths.h"
#include "log.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const char *const MINE[] = { "scan", "boxart", "cloud-sync", "backup-agent", "update-guard", NULL };

static void path(const char *name, char *o, size_t n) { snprintf(o, n, "%s/phosphor/state/busy/%s", p_fat(), name); }

static const char *boot_id(void)
{
	static char id[64];
	if (!id[0]) {
		FILE *f = fopen("/proc/sys/kernel/random/boot_id", "r");
		if (f) { if (!fgets(id, sizeof(id), f)) id[0] = 0; fclose(f); }
		id[strcspn(id, "\n")] = 0;
	}
	return id;
}

static int read_boot(const char *p, char *o, size_t n)
{
	FILE *f = fopen(p, "r");
	if (!f) return -1;
	char line[160];
	int found = -1;
	while (fgets(line, sizeof(line), f))
		if (!strncmp(line, "boot=", 5)) { snprintf(o, n, "%.63s", line + 5); o[strcspn(o, "\n")] = 0; found = 0; break; }
	fclose(f);
	return found;
}

void busy_init(void)
{
	char p[600], b[64];
	for (int i = 0; MINE[i]; i++) {
		path(MINE[i], p, sizeof(p));
		if (!read_boot(p, b, sizeof(b)) && !strcmp(b, boot_id())) unlink(p);
	}
}

void busy_mark(const char *name, const char *what)
{
	if (!boot_id()[0]) return;
	char p[600], dir[600], buf[300];
	snprintf(dir, sizeof(dir), "%s/phosphor/state/busy", p_fat());
	if (mkdirs(dir)) return;
	path(name, p, sizeof(p));
	int n = snprintf(buf, sizeof(buf), "boot=%s\nat=%ld\nwhat=%s\n", boot_id(), (long)time(NULL), what);
	int fd = open(p, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (fd < 0) return;
	if (write(fd, buf, (size_t)n) != n) plog("busy: couldn't write %s", p);
	fsync(fd);
	close(fd);
	int d = open(dir, O_RDONLY | O_CLOEXEC);
	if (d >= 0) { fsync(d); close(d); }
}

void busy_clear(const char *name)
{
	char p[600];
	path(name, p, sizeof(p));
	unlink(p);
}
