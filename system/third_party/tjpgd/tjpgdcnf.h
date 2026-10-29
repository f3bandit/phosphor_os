/* TJpgDec configuration for phosphord's wallpaper thumbnails (Phosphor; the decoder is ChaN's, from
   LVGL's copy): RGB888 out, scaled decoding (1/2, 1/4, 1/8: a 4K JPEG at 1/8 is only its DC values), the
   fastest Huffman decoding (a 32-bit CPU with RAM to spare) */
#define JD_SZBUF        4096
#define JD_FORMAT       0
#define JD_USE_SCALE    1
#define JD_TBLCLIP      1
#define JD_FASTDECODE   2
