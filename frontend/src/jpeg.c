/* Phosphor shell: JPEG box art, decoded whole (stb_image).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * phosphord stores box art as JPEG (about 60 KB a cover instead of 500 KB as PNG). LVGL's own
 * JPEG decoder (TJPGD) hands out tiles, which can't be scaled to fit the details panel, so covers
 * are decoded in one go here instead, into the same ARGB8888 buffer the PNG decoder gives. */
#include "jpeg.h"
#include "lvgl.h"
#include "src/lvgl_private.h"   /* decoder internals, as LVGL's own PNG decoder uses */

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_MAX_DIMENSIONS 4096     /* stored covers are 512 px; anything bigger isn't ours */
#include "stb_image.h"

#define MAX_FILE (4 * 1024 * 1024)

static int is_jpeg_name(const char *fn)
{
	const char *ext = lv_fs_get_ext(fn);
	return !lv_strcmp(ext, "jpg") || !lv_strcmp(ext, "jpeg") || !lv_strcmp(ext, "JPG") || !lv_strcmp(ext, "JPEG");
}

static unsigned char *read_all(lv_fs_file_t *f, uint32_t *len)
{
	uint32_t size = 0, rn = 0;
	if (lv_fs_seek(f, 0, LV_FS_SEEK_END) != LV_FS_RES_OK || lv_fs_tell(f, &size) != LV_FS_RES_OK) return NULL;
	if (!size || size > MAX_FILE) return NULL;
	lv_fs_seek(f, 0, LV_FS_SEEK_SET);
	unsigned char *buf = lv_malloc(size);
	if (!buf) return NULL;
	if (lv_fs_read(f, buf, size, &rn) != LV_FS_RES_OK || rn != size) { lv_free(buf); return NULL; }
	*len = size;
	return buf;
}

static lv_result_t info(lv_image_decoder_t *dec, lv_image_decoder_dsc_t *dsc, lv_image_header_t *header)
{
	LV_UNUSED(dec);
	if (dsc->src_type != LV_IMAGE_SRC_FILE || !is_jpeg_name(dsc->src)) return LV_RESULT_INVALID;
	uint32_t len;
	unsigned char *buf = read_all(&dsc->file, &len);
	if (!buf) return LV_RESULT_INVALID;
	int w, h, n;
	int ok = stbi_info_from_memory(buf, (int)len, &w, &h, &n);
	lv_free(buf);
	if (!ok) return LV_RESULT_INVALID;
	header->cf = LV_COLOR_FORMAT_ARGB8888;
	header->w = (uint32_t)w;
	header->h = (uint32_t)h;
	header->stride = (uint32_t)w * 4;
	return LV_RESULT_OK;
}

static lv_result_t open_(lv_image_decoder_t *dec, lv_image_decoder_dsc_t *dsc)
{
	if (dsc->src_type != LV_IMAGE_SRC_FILE) return LV_RESULT_INVALID;
	lv_fs_file_t f;
	if (lv_fs_open(&f, dsc->src, LV_FS_MODE_RD) != LV_FS_RES_OK) return LV_RESULT_INVALID;
	uint32_t len;
	unsigned char *buf = read_all(&f, &len);
	lv_fs_close(&f);
	if (!buf) return LV_RESULT_INVALID;
	int w, h, n;
	unsigned char *px = stbi_load_from_memory(buf, (int)len, &w, &h, &n, 3);
	lv_free(buf);
	if (!px) { LV_LOG_WARN("jpeg: %s", stbi_failure_reason()); return LV_RESULT_INVALID; }
	lv_draw_buf_t *db = lv_draw_buf_create((uint32_t)w, (uint32_t)h, LV_COLOR_FORMAT_ARGB8888, 0);
	if (!db) { stbi_image_free(px); return LV_RESULT_INVALID; }
	for (int y = 0; y < h; y++) {
		uint8_t *row = db->data + (size_t)y * db->header.stride;
		const unsigned char *s = px + (size_t)y * w * 3;
		for (int x = 0; x < w; x++) {          /* RGB -> LVGL's BGRA */
			row[x * 4 + 0] = s[x * 3 + 2];
			row[x * 4 + 1] = s[x * 3 + 1];
			row[x * 4 + 2] = s[x * 3 + 0];
			row[x * 4 + 3] = 0xff;
		}
	}
	stbi_image_free(px);
	dsc->decoded = db;
	if (dsc->args.no_cache || !lv_image_cache_is_enabled()) return LV_RESULT_OK;
	lv_image_cache_data_t key;
	key.src_type = dsc->src_type;
	key.src = dsc->src;
	key.slot.size = db->data_size;
	lv_cache_entry_t *entry = lv_image_decoder_add_to_cache(dec, &key, db, NULL);
	if (!entry) { lv_draw_buf_destroy(db); dsc->decoded = NULL; return LV_RESULT_INVALID; }
	dsc->cache_entry = entry;
	return LV_RESULT_OK;
}

static void close_(lv_image_decoder_t *dec, lv_image_decoder_dsc_t *dsc)
{
	LV_UNUSED(dec);
	if (dsc->args.no_cache || !lv_image_cache_is_enabled()) lv_draw_buf_destroy((lv_draw_buf_t *)dsc->decoded);
}

void jpeg_init(void)
{
	lv_image_decoder_t *d = lv_image_decoder_create();
	lv_image_decoder_set_info_cb(d, info);
	lv_image_decoder_set_open_cb(d, open_);
	lv_image_decoder_set_close_cb(d, close_);
	d->name = "stb_jpeg";
}
