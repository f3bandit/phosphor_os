/* phosphord: Bluetooth controllers from the menu (pair, list, disconnect, forget) with MiSTer's own tools.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_BT_H
#define PHOSD_BT_H

#include <stddef.h>
#include <time.h>

#define BT_MAX 24

typedef struct {
	char mac[18], name[64], icon[32];
	int connected, input;
} bt_dev_t;

typedef struct {
	int present;           /* a Bluetooth adapter */
	int busy;              /* pairing running */
	int ok;                /* the last pairing worked (valid when !busy and at) */
	time_t at;             /* when the last pairing ended, 0 = none yet */
	char found[64];        /* the controller it found ("name (mac)") */
	char message[160];     /* what's happening / what happened */
} bt_state_t;

void bt_state(bt_state_t *s);
int bt_devices(bt_dev_t *out, int max);            /* paired devices */
int bt_pair_start(void);                           /* background, up to 90 s; 1 if already pairing, -1 no adapter */
void bt_pair_cancel(void);
int bt_forget(const char *mac, char *err, size_t n);
int bt_disconnect(const char *mac, char *err, size_t n);

#endif
