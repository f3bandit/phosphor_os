/* Windows build of exfatprogs: no ioctl. The block device calls go through win32_dev.c. */
#pragma once
#define BLKSSZGET 0x1268
#define BLKDISCARD 0x1277
#define ioctl(fd, req, arg) w_ioctl((fd), (req), (arg))
int w_ioctl(int fd, unsigned long req, void *arg);
