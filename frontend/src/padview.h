/* Phosphor shell: a controller's button layout, drawn the way SS1 Tool draws it (its layouts, in
 * padlayouts.h), live: pressed buttons light up, sticks move, the button being mapped glows.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_PADVIEW_H
#define PHOS_PADVIEW_H

#include "lvgl.h"

typedef struct padview padview_t;

/* a drawing w wide (600 x 345 proportions) at x, y in parent; starts as layout "generic" */
padview_t *padview_create(lv_obj_t *parent, int x, int y, int w);
void padview_delete(padview_t *p);          /* (lv_obj_clean of the parent frees the objects too) */
lv_obj_t *padview_obj(padview_t *p);         /* to hide or move it */
void padview_layout(padview_t *p, const char *key);
/* what the controller has (NULL: draw everything normally), what's pressed now, the button asked
   for (glows; NULL none), the buttons already mapped; sticks -1..1, triggers 0..1 */
typedef struct {
	const char *has[40]; int nhas;                 /* nhas < 0: unknown (draw everything) */
	const char *on[40]; int non;
	const char *target;
	const char *done[40]; int ndone;
	float lx, ly, rx, ry, l2, r2;
} padstate_t;
void padview_state(padview_t *p, const padstate_t *s);

/* the layouts: key, name, group ("Modern", "Retro", "Fight") */
int padview_count(void);
const char *padview_key(int i);
const char *padview_name(const char *key);
int padview_index(const char *key);         /* -1: not a layout */

#endif
