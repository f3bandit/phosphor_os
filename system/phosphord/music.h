/* phosphord: the music player (music.c).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOSD_MUSIC_H
#define PHOSD_MUSIC_H

#include "cJSON.h"
#include <stddef.h>

void music_init(void);
void music_scan(void);                       /* the Music folders again */
int music_home(char *out, size_t n);         /* where new albums go */
cJSON *music_albums(void);
cJSON *music_album(const char *id);          /* "cd": the music CD in the drive */
cJSON *music_cd(void);                       /* NULL: no music CD */
void music_cd_check(void);                   /* the drive changed */
int music_play(const char *album, int track, char *err, size_t errn);
int music_control(const char *what, cJSON *req, char *err, size_t errn);
void music_stop(void);                       /* a game is starting */
int music_playing(void);
cJSON *music_status(void);
cJSON *music_queue(void);
int music_rip(int start, char *err, size_t errn);     /* the CD in the drive to FLAC (0: stop) */
cJSON *music_rip_status(void);
void music_tick(const char *drive_key, int audio_cd);   /* from phosphord's loop */

#endif
