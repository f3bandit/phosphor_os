/* Windows build: exfatprogs asks for the Linux kernel version (>= 2.6, for sysfs); Windows has none. */
#pragma once
#include <string.h>
struct utsname { char sysname[65], release[65]; };
static inline int uname(struct utsname *u) { strcpy(u->sysname, "Windows"); strcpy(u->release, "6.0.0"); return 0; }
