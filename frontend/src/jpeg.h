/* Phosphor shell: JPEG box art, decoded whole (stb_image).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_JPEG_H
#define PHOS_JPEG_H
void jpeg_init(void);   /* after lv_init() */
struct _lv_draw_buf_t;
/* a JPEG file decoded once into its own buffer (not the image cache, which can drop it): the wallpaper.
   NULL if it can't be read; lv_draw_buf_destroy() it when done */
struct _lv_draw_buf_t *jpeg_load_buf(const char *path);
#endif
