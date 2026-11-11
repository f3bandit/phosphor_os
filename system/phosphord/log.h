/* phosphord: logging (stderr, <tmp>/phosphord.log, optionally the serial console).
 * Copyright (C) 2026 f3bandit. All rights reserved; licensed to Retro Remake (see LICENSE.md). */
#ifndef PHOSD_LOG_H
#define PHOSD_LOG_H

void log_open(const char *serial_dev);
void plog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
