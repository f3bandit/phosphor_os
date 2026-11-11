/* phosphord: the dashboard's numbers. Copyright (C) 2026 f3bandit. All rights reserved; licensed to Retro Remake (see LICENSE.md). */
#ifndef PHOSD_DASH_H
#define PHOSD_DASH_H
#include "cJSON.h"
/* temps, cpu (cumulative), load, uptime, mem, disks, io, ips, net, procs, hz, core; kernel log if asked */
cJSON *dash_json(int with_kernel);
#endif
