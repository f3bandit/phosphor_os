/* phosphord: MiSTer's own file sharing (Samba) on and off from the menu.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * MiSTer starts Samba at boot when linux/samba.sh exists (/etc/init.d/S91smb); the release ships it
 * as linux/_samba.sh (off). On = copy _samba.sh to samba.sh and start it now; off = move samba.sh
 * to phosphor/backups/samba/ and stop it. _samba.sh itself is never changed. */
#include "samba.h"
#include "log.h"
#include "paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int run(const char *cmd)
{
	int r = system(cmd);
	return (r != -1 && WIFEXITED(r)) ? WEXITSTATUS(r) : -1;
}

static int smbd_running(void) { return run("pidof smbd > /dev/null 2>&1") == 0; }

void samba_state(samba_state_t *s)
{
	char p[512];
	memset(s, 0, sizeof(*s));
	snprintf(p, sizeof(p), "%s/linux/samba.sh", p_fat());
	s->on = file_exists(p);
	snprintf(p, sizeof(p), "%s/linux/_samba.sh", p_fat());
	s->available = (s->on || file_exists(p)) && file_exists("/etc/init.d/S91smb");
	s->running = smbd_running();
}

static int copy_file(const char *from, const char *to)
{
	FILE *in = fopen(from, "rb");
	if (!in) return -1;
	char tmp[600];
	snprintf(tmp, sizeof(tmp), "%s.new", to);
	FILE *out = fopen(tmp, "wb");
	if (!out) { fclose(in); return -1; }
	char buf[8192];
	size_t n;
	int bad = 0;
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0) if (fwrite(buf, 1, n, out) != n) bad = 1;
	fclose(in);
	if (fclose(out) != 0) bad = 1;
	if (bad || rename(tmp, to) != 0) { unlink(tmp); return -1; }
	chmod(to, 0755);
	return 0;
}

int samba_set(int on, char *err, size_t n)
{
	char sh[512], off[512];
	snprintf(sh, sizeof(sh), "%s/linux/samba.sh", p_fat());
	snprintf(off, sizeof(off), "%s/linux/_samba.sh", p_fat());
	if (!file_exists("/etc/init.d/S91smb")) { snprintf(err, n, "This MiSTer Linux has no Samba"); return -1; }
	if (on) {
		if (!file_exists(sh)) {
			if (!file_exists(off)) { snprintf(err, n, "linux/_samba.sh isn't on the SD card"); return -1; }
			if (copy_file(off, sh) != 0) { snprintf(err, n, "Couldn't write linux/samba.sh"); return -1; }
		}
		if (!smbd_running()) run("/etc/init.d/S91smb start > /dev/null 2>&1");
		plog("samba: on (%s)", smbd_running() ? "running" : "didn't start");
		if (!smbd_running()) { snprintf(err, n, "Samba didn't start"); return -1; }
		return 0;
	}
	if (file_exists(sh)) {
		char dir[512], to[600];
		snprintf(dir, sizeof(dir), "%s/phosphor/backups/samba", p_fat());
		mkdirs(dir);
		time_t t = time(NULL);
		struct tm tm;
		localtime_r(&t, &tm);
		char stamp[32];
		strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tm);
		snprintf(to, sizeof(to), "%s/samba.sh.%s", dir, stamp);
		if (copy_file(sh, to) != 0 || unlink(sh) != 0) { snprintf(err, n, "Couldn't move linux/samba.sh aside"); return -1; }
	}
	if (smbd_running()) run("/etc/init.d/S91smb stop > /dev/null 2>&1");
	plog("samba: off");
	return 0;
}
