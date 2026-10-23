/* phosphord: a heartbeat on the SD card (testing aid): every 10 s the uptime, load, free memory and how
 * busy the SD card has been, so after a sudden reset the last lines show what the system was doing.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *   logs/heartbeat.log, kept to the last ~2000 lines */
#include "heartbeat.h"
#include "paths.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static long meminfo(const char *key)
{
	FILE *f = fopen("/proc/meminfo", "r");
	if (!f) return -1;
	char line[128];
	long v = -1;
	size_t n = strlen(key);
	while (fgets(line, sizeof(line), f)) if (!strncmp(line, key, n)) { sscanf(line + n, "%ld", &v); break; }
	fclose(f);
	return v;
}

/* sectors written to the SD card so far */
static long long sd_written(void)
{
	FILE *f = fopen("/sys/block/mmcblk0/stat", "r");
	long long v[8] = { 0 };
	if (f) { if (fscanf(f, "%lld %lld %lld %lld %lld %lld %lld", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6]) != 7) v[6] = 0; fclose(f); }
	return v[6];
}

static void *beat(void *arg)
{
	(void)arg;
	char path[512], old[520];
	snprintf(path, sizeof(path), "%s/phosphor/logs/heartbeat.log", p_fat());
	snprintf(old, sizeof(old), "%s.1", path);
	long long last_w = sd_written();
	for (int n = 0;; n++) {
		double up = 0, l1 = 0, l5 = 0;
		FILE *f = fopen("/proc/uptime", "r");
		if (f) { if (fscanf(f, "%lf", &up) != 1) up = 0; fclose(f); }
		f = fopen("/proc/loadavg", "r");
		if (f) { if (fscanf(f, "%lf %lf", &l1, &l5) != 2) l1 = 0; fclose(f); }
		long long w = sd_written();
		time_t t = time(NULL);
		struct tm tm;
		localtime_r(&t, &tm);
		char when[32];
		strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", &tm);
		if (n && n % 2000 == 0) rename(path, old);
		f = fopen(path, "a");
		if (f) {
			fprintf(f, "%s  up %.0fs  load %.2f %.2f  mem avail %ld KB free %ld KB  sd written %lld KB/10s\n", when, up, l1, l5,
				meminfo("MemAvailable:"), meminfo("MemFree:"), (w - last_w) / 2);
			fclose(f);
		}
		last_w = w;
		sleep(10);
	}
	return NULL;
}

void heartbeat_start(void)
{
	pthread_t t;
	if (pthread_create(&t, NULL, beat, NULL) == 0) pthread_detach(t);
}
