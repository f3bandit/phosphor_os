/* Phosphor frontend: live system state for the status bar.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Everything comes from /sys and /proc; nothing is spawned. PHOS_FAKE_STATUS
 * ("share=1,eth=0,wifi=2,bt=2") overrides it for screenshots. */
#include "sysinfo.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int read_line(const char *path, char *buf, size_t n)
{
	FILE *f = fopen(path, "r");
	if (!f) return 0;
	int ok = fgets(buf, (int)n, f) != NULL;
	fclose(f);
	if (ok) buf[strcspn(buf, "\n")] = 0;
	return ok;
}

static int fake_value(const char *key, int *out)
{
	const char *f = getenv("PHOS_FAKE_STATUS");
	if (!f) return 0;
	const char *p = strstr(f, key);
	if (!p || p[strlen(key)] != '=') return 0;
	*out = atoi(p + strlen(key) + 1);
	return 1;
}

static int share_mounted(void)
{
	FILE *f = fopen("/proc/self/mounts", "r");
	if (!f) return 0;
	char dev[512], mnt[512], type[64];
	int found = 0;
	while (fscanf(f, "%511s %511s %63s %*[^\n]", dev, mnt, type) == 3) {
		if (!strcmp(type, "cifs") || !strcmp(type, "smb3") || !strncmp(type, "nfs", 3)) { found = 1; break; }
	}
	fclose(f);
	return found;
}

/* wired: any non-wireless interface with carrier, other than lo */
static int ethernet_up(void)
{
	DIR *d = opendir("/sys/class/net");
	if (!d) return 0;
	struct dirent *e;
	int up = 0;
	char path[300], v[32];
	while ((e = readdir(d)) && !up) {
		if (e->d_name[0] == '.' || !strcmp(e->d_name, "lo")) continue;
		snprintf(path, sizeof(path), "/sys/class/net/%s/wireless", e->d_name);
		if (access(path, F_OK) == 0) continue;
		snprintf(path, sizeof(path), "/sys/class/net/%s/device", e->d_name);
		if (access(path, F_OK) != 0) continue;   /* virtual interface */
		snprintf(path, sizeof(path), "/sys/class/net/%s/carrier", e->d_name);
		if (read_line(path, v, sizeof(v)) && v[0] == '1') up = 1;
	}
	closedir(d);
	return up;
}

/* WiFi: bars from link quality in /proc/net/wireless (-1 if there's no adapter) */
static int wifi_bars(void)
{
	FILE *f = fopen("/proc/net/wireless", "r");
	if (!f) return -1;
	char line[256];
	int bars = -1;
	int n = 0;
	while (fgets(line, sizeof(line), f)) {
		if (++n <= 2) continue;   /* headers */
		char ifn[32];
		float link = 0;
		if (sscanf(line, " %31[^:]: %*s %f", ifn, &link) == 2) {
			char path[128], st[32];
			snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", ifn);
			int up = read_line(path, st, sizeof(st)) && !strcmp(st, "up");
			int b = !up ? 0 : link >= 50 ? 3 : link >= 30 ? 2 : 1;   /* link is out of 70 */
			if (b > bars) bars = b;
		}
	}
	fclose(f);
	if (bars < 0) {
		/* adapter present but not listed yet */
		DIR *d = opendir("/sys/class/net");
		if (d) {
			struct dirent *e;
			char path[300];
			while ((e = readdir(d))) {
				snprintf(path, sizeof(path), "/sys/class/net/%s/wireless", e->d_name);
				if (e->d_name[0] != '.' && access(path, F_OK) == 0) { bars = 0; break; }
			}
			closedir(d);
		}
	}
	return bars;
}

/* Bluetooth: connections show up as hciN:handle entries under the adapter */
static int bt_connected(void)
{
	DIR *d = opendir("/sys/class/bluetooth");
	if (!d) return -1;
	struct dirent *e;
	int adapters = 0, conns = 0;
	while ((e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		if (strchr(e->d_name, ':')) conns++;
		else adapters++;
	}
	closedir(d);
	return adapters ? conns : -1;
}

void sysinfo_read(sysinfo_t *s)
{
	s->share = share_mounted();
	s->ethernet = ethernet_up();
	s->wifi = wifi_bars();
	s->bt = bt_connected();
	fake_value("share", &s->share);
	fake_value("eth", &s->ethernet);
	fake_value("wifi", &s->wifi);
	fake_value("bt", &s->bt);
}
