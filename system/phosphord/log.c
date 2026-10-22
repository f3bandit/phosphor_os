/* phosphord: logging (stderr, <tmp>/phosphord.log, optionally the serial console).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#include "log.h"
#include "paths.h"
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static FILE *logf_, *serial_;
static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;

void log_open(const char *serial_dev)
{
	char p[600];
	mkdirs(p_tmp());
	snprintf(p, sizeof(p), "%s/phosphord.log", p_tmp());
	logf_ = fopen(p, "a");
	if (logf_) setvbuf(logf_, NULL, _IOLBF, 0);
	if (serial_dev) {
		serial_ = fopen(serial_dev, "w");
		if (serial_) setvbuf(serial_, NULL, _IOLBF, 0);
	}
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
	pthread_mutex_lock(&lk);
	fprintf(stderr, "[%5ld.%03ld] phosphord: %s\n", (long)ts.tv_sec, ts.tv_nsec / 1000000, msg);
	if (logf_) fprintf(logf_, "[%5ld.%03ld] %s\n", (long)ts.tv_sec, ts.tv_nsec / 1000000, msg);
	if (serial_) fprintf(serial_, "[%5ld.%03ld] phosphord: %s\r\n", (long)ts.tv_sec, ts.tv_nsec / 1000000, msg);
	pthread_mutex_unlock(&lk);
}
