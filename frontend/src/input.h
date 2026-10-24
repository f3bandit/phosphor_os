/* Phosphor frontend: controller and keyboard input (Linux evdev), with hot-plug and key repeat.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_INPUT_H
#define PHOS_INPUT_H

typedef enum {
	KEY_NONE_, K_UP, K_DOWN, K_LEFT, K_RIGHT,
	K_A,        /* confirm (south face button, Enter) */
	K_B,        /* back (east face button, Esc/Backspace) */
	K_X, K_Y,
	K_MENU,     /* Start / Guide / F1 */
	K_SELECT,
	K_L, K_R,   /* shoulders, PgUp/PgDn: jump a page */
	K_POWER,    /* Select + Start together: the power window (on their own they are Select and Menu) */
} pkey_t;

void input_init(void);
#include <poll.h>
/* the fds to wait on (devices, hot-plug, remote keys); returns how many were put in p */
int input_pollfds(struct pollfd *p, int max);
/* a direction is held: keys repeat, so the loop must keep coming round */
int input_held(void);
/* next key press (with repeat for held directions), or KEY_NONE_; call every frame */
pkey_t input_poll(unsigned now_ms);
/* the key input_poll just gave came from the Windows app (Remote control) */
int input_from_app(void);
/* inject keys for tests and screenshots */
void input_push(pkey_t k);
/* a controller button from MiSTer Main ("up", "a", "start"...; p 1 pressed, 2 held/repeat, 0 released).
 * From the first one on, controllers are only taken from Main, like MiSTer's own menu */
void input_main_pad(const char *button, int p);
pkey_t key_from_name(const char *name);
/* a controller or keyboard was just plugged in or paired ("Connected: name") or went away; NULL if
 * nothing new. Each one is returned once. */
const char *input_notice(void);
/* the input watch log, /tmp/phosphor-input.log (RAM): every button the shell gets and where from, so
 * input can be checked from Phosphor Link. Remote keys: write names ("up", "a", "start"...) one a line
 * to the FIFO /tmp/phosphor-keys */
void input_watch(const char *src, const char *what);

#endif
