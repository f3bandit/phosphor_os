/* phosphord: the SuperStation One's front ports (controllers, memory cards) through phosphor-main.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_MEMCARD_H
#define PHOSD_MEMCARD_H

#include <stddef.h>

typedef struct {
	int main_api;         /* phosphor-main is running (its socket answers) */
	int available;        /* the Phosphor menu core is loaded */
	int can_load;         /* it isn't, but it's installed and we're in the menu */
	int in_game;
	int on;
	int busy;             /* a card job is running */
	char job[16];         /* backup | restore | check */
	int port;
	int done, total;      /* sectors */
	int ok;               /* the last job worked (valid when !busy and job[0]) */
	char message[200];    /* what happened, for the screen */
	char file[128];       /* the backup it wrote / the file it restored */
} ports_state_t;

void memcard_start(void);
void memcard_state(ports_state_t *s);
/* load the Phosphor menu core if needed and turn the ports on; 0 ok, else err filled */
int ports_prepare(char *err, size_t n);
/* {"ok":true,"on":..,"pads":[...]} from Main, as is; NULL + err when the ports aren't there */
char *ports_pads(char *err, size_t n);
int ports_pad_analog(int port, char *err, size_t n);
/* start a job: "backup" (whole card to <data>/memcards), "restore" (a backup back onto the card:
 * backs the card up first, writes, reads every sector back), "check" (is there a card, formatted?).
 * 0 started, 1 busy, -1 err filled */
int memcard_job(const char *what, int port, const char *name, char *err, size_t n);
/* the backups: name\tbytes\tmtime lines, newest first; caller frees */
char *memcard_list(void);
const char *memcard_dir(void);

#endif
