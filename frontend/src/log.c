/* Phosphor frontend: logging to stderr, a file in /tmp, and optionally the serial console.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

static FILE *logf_, *serial_;

void log_open(const char *serial_dev)
{
	mkdir("/tmp/phosphor", 0755);
	logf_ = fopen("/tmp/phosphor/frontend.log", "a");
	if (serial_dev) serial_ = fopen(serial_dev, "w");
	if (logf_) setvbuf(logf_, NULL, _IOLBF, 0);
	if (serial_) setvbuf(serial_, NULL, _IOLBF, 0);
}

void plog(const char *fmt, ...)
{
	char msg[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	char line[1100];
	snprintf(line, sizeof(line), "[%5ld.%03ld] phosphor: %s\n", (long)ts.tv_sec, ts.tv_nsec / 1000000, msg);
	fputs(line, stderr);
	if (logf_) fputs(line, logf_);
	if (serial_) {
		/* a serial console wants CR LF */
		fprintf(serial_, "[%5ld.%03ld] phosphor: %s\r\n", (long)ts.tv_sec, ts.tv_nsec / 1000000, msg);
	}
}
