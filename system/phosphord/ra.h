/* phosphord: RetroAchievements (ra.c).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_RA_H
#define PHOSD_RA_H

#include "cJSON.h"
#include "sqlite3.h"
#include <stddef.h>

void ra_init(void);
cJSON *ra_status(void);
int ra_login(const char *user, const char *password, char *err, size_t errn);   /* keeps the token, not the password */
void ra_logout(void);
int ra_logged_in(void);
int ra_set(cJSON *req, char *err, size_t errn);           /* hardcore, popups, sound */
int ra_set_key(const char *key, char *err, size_t errn);  /* the Web API key ("" removes it) */
int ra_get_cores(char *err, size_t errn);                 /* odelot's cores from GitHub, in the background */
/* the RetroAchievements core for a system, when logged in and it's there: rbf (for the .mgl) and set name */
int ra_core_for(const char *sys, char *rbf, size_t rn, char *setname, size_t sn);
int ra_match(char *err, size_t errn);                     /* match the library to RetroAchievements, in the background */
/* the viewer, answered at once from the last result; *updating while a fresh one is fetched in the background.
 * NULL with err when there's none yet (with *updating: still asking) */
cJSON *ra_profile(char *err, size_t errn, int *updating);
cJSON *ra_game(sqlite3 *db, long long game_id, char *err, size_t errn, int *updating);
void ra_badge(sqlite3 *db, long long game_id, cJSON *g);  /* adds "ra":{total,unlocked} when known */

#endif
