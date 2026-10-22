/* phosphord: logging (stderr, <tmp>/phosphord.log, optionally the serial console).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_LOG_H
#define PHOSD_LOG_H

void log_open(const char *serial_dev);
void plog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
