/* Windows build of exfatprogs' fsck.exfat (Phosphor for Windows, SD card repair).
 * Copyright (C) 2026 f3bandit. GPL-2.0-or-later, as exfatprogs.
 *
 * Included before every exfatprogs source file (-include). exfatprogs is left as it is upstream: the
 * calls it makes on the drive (open, pread, pwrite, fsync, lseek, fstat, ioctl, close) go to
 * win32_dev.c, which opens a Windows volume ("E:") locked and dismounted, or an image file, and does
 * the sector-aligned reads and writes a raw Windows volume needs. */
#pragma once
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <unistd.h>

/* the config.h autoconf would write */
#define PACKAGE "exfatprogs"

int w_open(const char *path, int flags, ...);
ssize_t w_pread(int fd, void *buf, size_t n, off_t off);
ssize_t w_pwrite(int fd, const void *buf, size_t n, off_t off);
int w_fsync(int fd);
off_t w_lseek(int fd, off_t off, int whence);
int w_fstat(int fd, struct stat *st);
int w_close(int fd);

#define open w_open
#define pread w_pread
#define pwrite w_pwrite
#define fsync w_fsync
#undef lseek
#define lseek w_lseek
#undef fstat
#define fstat(fd, st) w_fstat((fd), (st))
#define close w_close

/* Linux-only calls on code paths a Windows volume never takes (sysfs, a real block device) */
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif
#ifndef AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_NOFOLLOW 0x100
#endif
#ifndef S_ISBLK
#define S_ISBLK(m) 0
#endif
#ifndef S_ISLNK
#define S_ISLNK(m) 0
#endif
#ifndef O_EXCL
#define O_EXCL 0
#endif
static inline int openat(int at, const char *p, int fl, ...) { (void)at; (void)p; (void)fl; errno = ENOSYS; return -1; }
static inline int fstatat(int at, const char *p, struct stat *s, int fl) { (void)at; (void)p; (void)s; (void)fl; errno = ENOSYS; return -1; }
#include <dirent.h>
static inline DIR *fdopendir(int fd) { (void)fd; errno = ENOSYS; return NULL; }

/* reads ahead (posix_fadvise): only a hint */
#define POSIX_FADV_WILLNEED 3
#define POSIX_FADV_DONTNEED 4
#define POSIX_FADV_SEQUENTIAL 2
#define posix_fadvise(fd, off, len, adv) 0
/* aligned buffers: win32_dev.c aligns every transfer itself, so plain malloc (freed with free) */
static inline int posix_memalign(void **p, size_t a, size_t n) { (void)a; *p = malloc(n); return *p ? 0 : ENOMEM; }

/* "long" is 32 bits on Windows (LLP64): exfatprogs' bitmaps then use 32-bit words */
#include <limits.h>
#ifndef ULONG_WIDTH
#define ULONG_WIDTH 32
#endif

/* gmtime_r (POSIX) is gmtime_s on Windows, arguments the other way round */
#include <time.h>
static inline struct tm *gmtime_r(const time_t *t, struct tm *out) { return gmtime_s(out, t) ? NULL : out; }

/* fcntl: only used to set O_DIRECT for a read-back check; a Windows volume is opened unbuffered anyway */
#define F_GETFL 3
#define F_SETFL 4
#ifndef O_DIRECT
#define O_DIRECT 0
#endif
static inline int fcntl(int fd, int cmd, ...) { (void)fd; (void)cmd; return 0; }
