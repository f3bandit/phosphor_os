/* Windows build of exfatprogs' fsck.exfat: the drive under the POSIX calls exfatprogs makes.
 * Copyright (C) 2026 f3bandit. GPL-2.0-or-later, as exfatprogs.
 *
 * The drive is named by its letter ("E:", "E:\", "\\.\E:") or is an image file (tests). A volume is
 * opened unbuffered and write-through, locked (nothing else may have it open: Windows says no
 * otherwise) and dismounted, so Windows forgets what it knew about the file system and reads it fresh
 * after the repair. Raw volume I/O must be whole sectors at sector offsets into sector-aligned memory:
 * every read and write goes through an aligned bounce buffer, partial sectors read first. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/ioctl.h>

#undef open
#undef pread
#undef pwrite
#undef fsync
#undef lseek
#undef fstat
#undef close

#define FD_BASE 1000
#define MAXDEV 4
#define CHUNK (1u << 20)                   /* the most moved in one call */

static struct dev { HANDLE h; int vol, locked, strict; DWORD ss; long long size, pos; unsigned char *bb; } devs[MAXDEV];

static struct dev *dev_of(int fd)
{
	int i = fd - FD_BASE;
	return i >= 0 && i < MAXDEV && devs[i].h ? &devs[i] : NULL;
}

static int win_errno(DWORD e)
{
	switch (e) {
	case ERROR_ACCESS_DENIED: case ERROR_SHARING_VIOLATION: case ERROR_LOCK_VIOLATION: return EBUSY;
	case ERROR_FILE_NOT_FOUND: case ERROR_PATH_NOT_FOUND: return ENOENT;
	case ERROR_WRITE_PROTECT: return EROFS;
	case ERROR_NOT_READY: return ENODEV;
	default: return EIO;
	}
}

/* "E:", "e:\", "\\.\E:" -> "\\.\E:"; anything else is a file */
static int volume_name(const char *p, char *out, size_t n)
{
	if (!strncmp(p, "\\\\.\\", 4)) p += 4;
	if (isalpha((unsigned char)p[0]) && p[1] == ':' && (!p[2] || ((p[2] == '\\' || p[2] == '/') && !p[3]))) {
		snprintf(out, n, "\\\\.\\%c:", toupper((unsigned char)p[0]));
		return 1;
	}
	return 0;
}

int w_open(const char *path, int flags, ...)
{
	int i, rw = (flags & O_ACCMODE) != O_RDONLY;
	char vol[16];
	struct dev *d;
	DWORD got;

	if (!strcmp(path, "/dev/zero")) { errno = ENOENT; return -1; }   /* (exfatprogs then uses calloc) */
	for (i = 0; i < MAXDEV && devs[i].h; i++) ;
	if (i == MAXDEV) { errno = EMFILE; return -1; }
	d = &devs[i];
	memset(d, 0, sizeof(*d));
	d->vol = volume_name(path, vol, sizeof(vol));
	d->h = CreateFileA(d->vol ? vol : path, GENERIC_READ | (rw ? GENERIC_WRITE : 0),
	                   FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
	                   d->vol ? FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH : FILE_ATTRIBUTE_NORMAL, NULL);
	if (d->h == INVALID_HANDLE_VALUE) { errno = win_errno(GetLastError()); d->h = NULL; return -1; }
	d->ss = 512;
	if (d->vol) {
		DISK_GEOMETRY_EX g;
		GET_LENGTH_INFORMATION li;
		if (DeviceIoControl(d->h, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &g, sizeof(g), &got, NULL)
		    && g.Geometry.BytesPerSector >= 512 && g.Geometry.BytesPerSector <= 4096)
			d->ss = g.Geometry.BytesPerSector;
		if (!DeviceIoControl(d->h, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &li, sizeof(li), &got, NULL)) {
			errno = win_errno(GetLastError()); CloseHandle(d->h); d->h = NULL; return -1;
		}
		d->size = li.Length.QuadPart;
		/* nothing else may have it open: a check on a volume Windows keeps writing to means nothing */
		/* (Explorer lets go of a card a moment after its window closes: a few tries) */
		for (int t = 0; t < 10 && !d->locked; t++) {
			if (DeviceIoControl(d->h, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &got, NULL)) d->locked = 1;
			else Sleep(500);
		}
		if (d->locked) {
			DeviceIoControl(d->h, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &got, NULL);
		} else if (rw) {
			fprintf(stderr, "The card is in use (a window or program has files on it open): close them and try again.\n");
			CloseHandle(d->h); d->h = NULL; errno = EBUSY; return -1;
		} else {
			fprintf(stderr, "Note: the card is in use, so a check may see changes still being written.\n");
		}
	} else {
		LARGE_INTEGER s;
		if (!GetFileSizeEx(d->h, &s)) { errno = EIO; CloseHandle(d->h); d->h = NULL; return -1; }
		d->size = s.QuadPart;
		/* tests: an image file under the same rules as a volume (whole sectors only, checked) */
		if (getenv("FSCK_STRICT_ALIGN")) { d->strict = 1; d->size &= ~(long long)511; }
	}
	d->bb = VirtualAlloc(NULL, CHUNK + 2 * 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!d->bb) { CloseHandle(d->h); d->h = NULL; errno = ENOMEM; return -1; }
	return FD_BASE + i;
}

/* whole sectors [off, off+n) at an aligned offset, into or out of the bounce buffer */
static int raw(struct dev *d, int wr, long long off, DWORD n)
{
	OVERLAPPED o;
	DWORD got = 0;
	if (d->strict && (off % d->ss || n % d->ss || ((uintptr_t)d->bb) % 4096)) {
		fprintf(stderr, "win32_dev: unaligned raw I/O (off %lld, len %lu)\n", off, (unsigned long)n);
		abort();
	}
	memset(&o, 0, sizeof(o));
	o.Offset = (DWORD)off;
	o.OffsetHigh = (DWORD)(off >> 32);
	BOOL ok = wr ? WriteFile(d->h, d->bb, n, &got, &o) : ReadFile(d->h, d->bb, n, &got, &o);
	if (!ok) { errno = win_errno(GetLastError()); return -1; }
	if (got != n) { errno = EIO; return -1; }
	return 0;
}

static ssize_t xfer(int fd, void *buf, size_t n, off_t off, int wr)
{
	struct dev *d = dev_of(fd);
	if (!d) { errno = EBADF; return -1; }
	if (off < 0) { errno = EINVAL; return -1; }
	if (off >= d->size) return 0;
	if ((long long)n > d->size - off) n = (size_t)(d->size - off);
	if (n > CHUNK) n = CHUNK;
	if (!n) return 0;
	long long a = off & ~(long long)(d->ss - 1);                     /* the sectors that hold it */
	long long e = (off + (long long)n + d->ss - 1) & ~(long long)(d->ss - 1);
	if (e > d->size && !d->strict && !d->vol) e = d->size;            /* (a file ending mid-sector) */
	DWORD len = (DWORD)(e - a), head = (DWORD)(off - a);
	if (!wr) {
		if (raw(d, 0, a, len)) return -1;
		memcpy(buf, d->bb + head, n);
		return (ssize_t)n;
	}
	if (head || (n + head) % d->ss) {                                 /* partial sectors: read them first */
		if (raw(d, 0, a, len)) return -1;
	}
	memcpy(d->bb + head, buf, n);
	if (raw(d, 1, a, len)) return -1;
	return (ssize_t)n;
}

ssize_t w_pread(int fd, void *buf, size_t n, off_t off) { return xfer(fd, buf, n, off, 0); }
ssize_t w_pwrite(int fd, const void *buf, size_t n, off_t off) { return xfer(fd, (void *)buf, n, off, 1); }

int w_fsync(int fd)
{
	struct dev *d = dev_of(fd);
	if (!d) return _commit(fd);
	if (!FlushFileBuffers(d->h)) { errno = win_errno(GetLastError()); return -1; }
	return 0;
}

off_t w_lseek(int fd, off_t off, int whence)
{
	struct dev *d = dev_of(fd);
	if (!d) return _lseeki64(fd, off, whence);
	long long p = whence == SEEK_END ? d->size + off : whence == SEEK_CUR ? d->pos + off : off;
	if (p < 0) { errno = EINVAL; return -1; }
	d->pos = p;
	return (off_t)p;
}

int w_fstat(int fd, struct stat *st)
{
	struct dev *d = dev_of(fd);
	if (!d) { errno = EBADF; return -1; }   /* (fsck only asks about the drive) */
	memset(st, 0, sizeof(*st));
	st->st_mode = S_IFREG;                  /* (not a Linux block device: no sysfs to look at) */
	st->st_size = d->size;
	return 0;
}

int w_ioctl(int fd, unsigned long req, void *arg)
{
	struct dev *d = dev_of(fd);
	if (!d) { errno = EBADF; return -1; }
	if (req == BLKSSZGET && d->vol) { *(int *)arg = (int)d->ss; return 0; }
	errno = ENOTTY;                         /* (BLKDISCARD: not used by fsck) */
	return -1;
}

int w_close(int fd)
{
	struct dev *d = dev_of(fd);
	if (!d) return _close(fd);
	DWORD got;
	FlushFileBuffers(d->h);
	if (d->locked) {
		DeviceIoControl(d->h, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &got, NULL);
	}
	CloseHandle(d->h);
	VirtualFree(d->bb, 0, MEM_RELEASE);
	memset(d, 0, sizeof(*d));
	return 0;
}
