/* phosphord: remote access over the home network (phone app, PC app, Claude plugin).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_REMOTE_H
#define PHOSD_REMOTE_H

#include <stddef.h>

#define REMOTE_PORT 8767
#define REMOTE_MAX_DEVICES 16

typedef struct {
	char id[17];            /* first 16 hex digits of the key's hash: names the device for "forget" */
	char name[64];
	long created, last_seen;
} remote_dev_t;

typedef struct {
	int on, port, listening;
	char error[128];        /* why it isn't listening, if it should be */
	char addr[4][48];       /* this MiSTer's addresses (IPv4) */
	int naddr;
	char code[8];           /* pairing code while pairing is open, else "" */
	int code_left;          /* seconds */
	char paired[64];        /* name of the last device paired while this pairing was open */
	remote_dev_t dev[REMOTE_MAX_DEVICES];
	int ndev;
} remote_info_t;

void remote_start(void);                 /* background thread; listens only while turned on */
void remote_info(remote_info_t *ri);
int remote_set_on(int on);               /* saved in config/remote.ini */
int remote_pair_start(void);             /* new 6-digit code, open for 2 minutes */
void remote_pair_stop(void);
int remote_forget(const char *id);       /* 0 ok, -1 no such device */

#endif
