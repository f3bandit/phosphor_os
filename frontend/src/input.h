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
} pkey_t;

void input_init(void);
/* next key press (with repeat for held directions), or KEY_NONE_; call every frame */
pkey_t input_poll(unsigned now_ms);
/* inject keys for tests and screenshots */
void input_push(pkey_t k);
pkey_t key_from_name(const char *name);

#endif
