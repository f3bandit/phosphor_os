/* phosphord: where things live.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Everything can be redirected for tests:
 *   PHOS_FAT          SD card root          (default /media/fat)
 *   PHOS_TMP          runtime folder        (default /tmp/phosphor)
 *   PHOS_GAMES_ROOTS  games roots, | separated (default: /media/usb0-7/games, <fat>/cifs/games, <fat>/games)
 *   PHOS_MISTER_CMD   Main's command pipe   (default /dev/MiSTer_cmd)
 *   PHOS_CORENAME     running core file     (default /tmp/CORENAME) */
#ifndef PHOSD_PATHS_H
#define PHOSD_PATHS_H

#include <stddef.h>

const char *p_fat(void);
const char *p_tmp(void);
const char *p_data(void);          /* <fat>/phosphor/data */
const char *p_config(void);        /* <fat>/phosphor/config */
const char *p_socket(void);        /* <tmp>/phosphord.sock */
const char *p_mister_cmd(void);
const char *p_corename(void);

/* games roots in MiSTer's own order (USB drives, the share, the SD card) */
int p_roots(char out[][256], int max);

int mkdirs(const char *dir);      /* mkdir -p */
int file_exists(const char *path);
int dir_exists(const char *path);

#endif
