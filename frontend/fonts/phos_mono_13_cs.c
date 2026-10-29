/*******************************************************************************
 * Size: 8 px
 * Bpp: 1
 * Opts: --no-compress --no-prefilter --bpp 1 --size 8 --font /usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf -r 0x20-0x7E,0xA0-0xFF,0x2014,0x2022,0x2026,0x2190-0x2195,0x2261 --format lvgl --lv-include lvgl.h -o fonts/phos_mono_13_cs.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef PHOS_MONO_13_CS
#define PHOS_MONO_13_CS 1
#endif

#if PHOS_MONO_13_CS

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xf2,

    /* U+0022 "\"" */
    0xf0,

    /* U+0023 "#" */
    0x32, 0xbe, 0xaf, 0x32, 0x80,

    /* U+0024 "$" */
    0x4e, 0xcc, 0x75, 0xe4, 0x40,

    /* U+0025 "%" */
    0xe5, 0x3c, 0xf2, 0x9c,

    /* U+0026 "&" */
    0x62, 0x11, 0x4a, 0xc9, 0xa0,

    /* U+0027 "'" */
    0xc0,

    /* U+0028 "(" */
    0x6a, 0xa9,

    /* U+0029 ")" */
    0xa5, 0x5a,

    /* U+002A "*" */
    0x23, 0x9c, 0x40,

    /* U+002B "+" */
    0x27, 0xc8, 0x40,

    /* U+002C "," */
    0xc0,

    /* U+002D "-" */
    0xc0,

    /* U+002E "." */
    0x80,

    /* U+002F "/" */
    0x11, 0x22, 0x44, 0x48,

    /* U+0030 "0" */
    0x69, 0x9b, 0x99, 0x60,

    /* U+0031 "1" */
    0xc9, 0x24, 0xb8,

    /* U+0032 "2" */
    0x61, 0x12, 0x24, 0xf0,

    /* U+0033 "3" */
    0xe1, 0x16, 0x11, 0xe0,

    /* U+0034 "4" */
    0x11, 0x94, 0xa9, 0x7c, 0x40,

    /* U+0035 "5" */
    0xd3, 0x12, 0x70,

    /* U+0036 "6" */
    0x68, 0xe9, 0x99, 0x60,

    /* U+0037 "7" */
    0xf1, 0x22, 0x24, 0x40,

    /* U+0038 "8" */
    0x69, 0x96, 0x99, 0x60,

    /* U+0039 "9" */
    0x69, 0x99, 0x71, 0x60,

    /* U+003A ":" */
    0x90,

    /* U+003B ";" */
    0x8c,

    /* U+003C "<" */
    0x16, 0x87,

    /* U+003D "=" */
    0xf0, 0xf0,

    /* U+003E ">" */
    0x86, 0x1e,

    /* U+003F "?" */
    0xe4, 0xa4, 0x10,

    /* U+0040 "@" */
    0x72, 0x6f, 0x9c, 0xdd, 0xe,

    /* U+0041 "A" */
    0x23, 0x14, 0xa7, 0x4a, 0x20,

    /* U+0042 "B" */
    0xe9, 0x9e, 0x99, 0xe0,

    /* U+0043 "C" */
    0x7c, 0x88, 0x8c, 0x70,

    /* U+0044 "D" */
    0xe9, 0x99, 0x99, 0xe0,

    /* U+0045 "E" */
    0xf8, 0x8f, 0x88, 0xf0,

    /* U+0046 "F" */
    0xf8, 0x8e, 0x88, 0x80,

    /* U+0047 "G" */
    0x78, 0x8b, 0x99, 0x70,

    /* U+0048 "H" */
    0x99, 0x9f, 0x99, 0x90,

    /* U+0049 "I" */
    0xe9, 0x24, 0xb8,

    /* U+004A "J" */
    0x71, 0x11, 0x19, 0x70,

    /* U+004B "K" */
    0x9a, 0xcc, 0xa9, 0x90,

    /* U+004C "L" */
    0x88, 0x88, 0x88, 0xf0,

    /* U+004D "M" */
    0x9f, 0xfd, 0x99, 0x90,

    /* U+004E "N" */
    0x9d, 0xdd, 0xbb, 0x90,

    /* U+004F "O" */
    0x69, 0x99, 0x99, 0x60,

    /* U+0050 "P" */
    0xe9, 0x99, 0xe8, 0x80,

    /* U+0051 "Q" */
    0x69, 0x99, 0x99, 0x61,

    /* U+0052 "R" */
    0xe4, 0xa5, 0xc9, 0x4a, 0x40,

    /* U+0053 "S" */
    0x78, 0x86, 0x11, 0xe0,

    /* U+0054 "T" */
    0xf9, 0x8, 0x42, 0x10, 0x80,

    /* U+0055 "U" */
    0x99, 0x99, 0x99, 0x60,

    /* U+0056 "V" */
    0x94, 0x94, 0xa5, 0x10, 0x80,

    /* U+0057 "W" */
    0x8c, 0x6b, 0xef, 0x29, 0x40,

    /* U+0058 "X" */
    0x92, 0x98, 0x46, 0x2a, 0x20,

    /* U+0059 "Y" */
    0x8a, 0x94, 0x42, 0x10, 0x80,

    /* U+005A "Z" */
    0xf1, 0x24, 0x48, 0xf0,

    /* U+005B "[" */
    0xea, 0xaa, 0xc0,

    /* U+005C "\\" */
    0x84, 0x42, 0x22, 0x11,

    /* U+005D "]" */
    0xd5, 0x55, 0xc0,

    /* U+005E "^" */
    0x23, 0x24,

    /* U+005F "_" */
    0xf8,

    /* U+0060 "`" */
    0x90,

    /* U+0061 "a" */
    0xe1, 0xf9, 0xf0,

    /* U+0062 "b" */
    0x88, 0xe9, 0x99, 0xe0,

    /* U+0063 "c" */
    0x78, 0x88, 0x70,

    /* U+0064 "d" */
    0x11, 0x79, 0x99, 0x70,

    /* U+0065 "e" */
    0x69, 0xf8, 0x70,

    /* U+0066 "f" */
    0x74, 0xf4, 0x44, 0x40,

    /* U+0067 "g" */
    0x79, 0x99, 0x71, 0x60,

    /* U+0068 "h" */
    0x93, 0xdb, 0x68,

    /* U+0069 "i" */
    0x43, 0x24, 0xb8,

    /* U+006A "j" */
    0x21, 0x92, 0x49, 0xe0,

    /* U+006B "k" */
    0x88, 0xac, 0xca, 0x90,

    /* U+006C "l" */
    0xc4, 0x44, 0x44, 0x70,

    /* U+006D "m" */
    0xfd, 0x6b, 0x5a, 0x80,

    /* U+006E "n" */
    0xf6, 0xda,

    /* U+006F "o" */
    0x69, 0x99, 0x60,

    /* U+0070 "p" */
    0xe9, 0x99, 0xe8, 0x80,

    /* U+0071 "q" */
    0x79, 0x99, 0x71, 0x10,

    /* U+0072 "r" */
    0xf2, 0x48,

    /* U+0073 "s" */
    0xf1, 0x1e,

    /* U+0074 "t" */
    0x44, 0xf4, 0x44, 0x70,

    /* U+0075 "u" */
    0xb6, 0xde,

    /* U+0076 "v" */
    0x95, 0x56, 0x20,

    /* U+0077 "w" */
    0x8d, 0x6d, 0xa5, 0x0,

    /* U+0078 "x" */
    0x53, 0x8, 0xa9, 0x0,

    /* U+0079 "y" */
    0x92, 0x94, 0xc2, 0x11, 0x0,

    /* U+007A "z" */
    0xf2, 0x48, 0xf0,

    /* U+007B "{" */
    0x69, 0x28, 0x92, 0x60,

    /* U+007C "|" */
    0xff, 0x80,

    /* U+007D "}" */
    0xc9, 0x22, 0x92, 0xc0,

    /* U+007E "~" */
    0xf0,

    /* U+00A0 " " */
    0x0,

    /* U+00A1 "¡" */
    0xbc,

    /* U+00A2 "¢" */
    0x22, 0x7a, 0xaa, 0x72,

    /* U+00A3 "£" */
    0x74, 0x4e, 0x44, 0xf0,

    /* U+00A4 "¤" */
    0xf7, 0x80,

    /* U+00A5 "¥" */
    0x8a, 0x95, 0xbf, 0x90, 0x80,

    /* U+00A6 "¦" */
    0xe7,

    /* U+00A7 "§" */
    0xf1, 0x5a, 0x8f,

    /* U+00A8 "¨" */
    0xa0,

    /* U+00A9 "©" */
    0x77, 0xf3, 0x9f, 0xb8,

    /* U+00AA "ª" */
    0xdf, 0xa0,

    /* U+00AB "«" */
    0x16, 0xa6,

    /* U+00AC "¬" */
    0xf1, 0x10,

    /* U+00AD "­" */
    0xc0,

    /* U+00AE "®" */
    0x77, 0xfb, 0xdd, 0xb8,

    /* U+00AF "¯" */
    0xe0,

    /* U+00B0 "°" */
    0xf7, 0x80,

    /* U+00B1 "±" */
    0x27, 0xc8, 0xf, 0x80,

    /* U+00B2 "²" */
    0x58,

    /* U+00B3 "³" */
    0x74,

    /* U+00B4 "´" */
    0x60,

    /* U+00B5 "µ" */
    0xaa, 0xaa, 0xf8, 0x80,

    /* U+00B6 "¶" */
    0x77, 0x77, 0x33, 0x33,

    /* U+00B7 "·" */
    0x80,

    /* U+00B8 "¸" */
    0x70,

    /* U+00B9 "¹" */
    0xc9, 0x0,

    /* U+00BA "º" */
    0xd6, 0xa0,

    /* U+00BB "»" */
    0xa, 0x5a,

    /* U+00BC "¼" */
    0x42, 0x11, 0xc7, 0x48, 0xc6, 0x10,

    /* U+00BD "½" */
    0x44, 0x4e, 0x7b, 0x12, 0x30,

    /* U+00BE "¾" */
    0xc2, 0x11, 0x87, 0x49, 0xce, 0x10,

    /* U+00BF "¿" */
    0x41, 0x29, 0x38,

    /* U+00C0 "À" */
    0x41, 0x8, 0xc5, 0x29, 0xd2, 0x88,

    /* U+00C1 "Á" */
    0x1, 0x8, 0xc5, 0x29, 0xd2, 0x88,

    /* U+00C2 "Â" */
    0x23, 0x8, 0xc5, 0x29, 0xd2, 0x88,

    /* U+00C3 "Ã" */
    0x71, 0x18, 0xa5, 0x3a, 0x51,

    /* U+00C4 "Ä" */
    0x51, 0x18, 0xa5, 0x3a, 0x51,

    /* U+00C5 "Å" */
    0x66, 0x66, 0x6a, 0xf9, 0x90,

    /* U+00C6 "Æ" */
    0x7a, 0xab, 0xea, 0xb0,

    /* U+00C7 "Ç" */
    0x78, 0x88, 0x88, 0x70, 0x20,

    /* U+00C8 "È" */
    0x44, 0xf8, 0x8f, 0x88, 0xf0,

    /* U+00C9 "É" */
    0x24, 0xf8, 0x8f, 0x88, 0xf0,

    /* U+00CA "Ê" */
    0x6, 0xf8, 0x8f, 0x88, 0xf0,

    /* U+00CB "Ë" */
    0x6f, 0x88, 0xf8, 0x8f,

    /* U+00CC "Ì" */
    0x8b, 0xa4, 0x92, 0xe0,

    /* U+00CD "Í" */
    0x2b, 0xa4, 0x92, 0xe0,

    /* U+00CE "Î" */
    0x57, 0xa4, 0x92, 0xe0,

    /* U+00CF "Ï" */
    0xbd, 0x24, 0x97,

    /* U+00D0 "Ð" */
    0xe9, 0x9d, 0x99, 0xe0,

    /* U+00D1 "Ñ" */
    0x69, 0xdd, 0xdb, 0xb9,

    /* U+00D2 "Ò" */
    0x40, 0x69, 0x99, 0x99, 0x60,

    /* U+00D3 "Ó" */
    0x20, 0x69, 0x99, 0x99, 0x60,

    /* U+00D4 "Ô" */
    0x6, 0x69, 0x99, 0x99, 0x60,

    /* U+00D5 "Õ" */
    0x6a, 0x69, 0x99, 0x99, 0x60,

    /* U+00D6 "Ö" */
    0x66, 0x99, 0x99, 0x96,

    /* U+00D7 "×" */
    0x35, 0x50,

    /* U+00D8 "Ø" */
    0x79, 0xbd, 0xd9, 0xe0,

    /* U+00D9 "Ù" */
    0x42, 0x99, 0x99, 0x99, 0x60,

    /* U+00DA "Ú" */
    0x24, 0x99, 0x99, 0x99, 0x60,

    /* U+00DB "Û" */
    0x6, 0x99, 0x99, 0x99, 0x60,

    /* U+00DC "Ü" */
    0x69, 0x99, 0x99, 0x96,

    /* U+00DD "Ý" */
    0x11, 0x22, 0xa5, 0x10, 0x84, 0x20,

    /* U+00DE "Þ" */
    0x88, 0xf9, 0x9f, 0x80,

    /* U+00DF "ß" */
    0xea, 0xcc, 0xb9, 0xf0,

    /* U+00E0 "à" */
    0x42, 0xe1, 0xf9, 0xf0,

    /* U+00E1 "á" */
    0x20, 0xe1, 0xf9, 0xf0,

    /* U+00E2 "â" */
    0x60, 0xe1, 0xf9, 0xf0,

    /* U+00E3 "ã" */
    0x6a, 0xe1, 0xf9, 0xf0,

    /* U+00E4 "ä" */
    0x60, 0xe1, 0xf9, 0xf0,

    /* U+00E5 "å" */
    0x66, 0x6e, 0x1f, 0x9f,

    /* U+00E6 "æ" */
    0xd9, 0x7f, 0x4d, 0x80,

    /* U+00E7 "ç" */
    0x78, 0x88, 0x62, 0x60,

    /* U+00E8 "è" */
    0x44, 0x26, 0x9f, 0x87,

    /* U+00E9 "é" */
    0x22, 0x6, 0x9f, 0x87,

    /* U+00EA "ê" */
    0x6, 0x6, 0x9f, 0x87,

    /* U+00EB "ë" */
    0x60, 0x69, 0xf8, 0x70,

    /* U+00EC "ì" */
    0x8b, 0x24, 0xb8,

    /* U+00ED "í" */
    0x43, 0x24, 0xb8,

    /* U+00EE "î" */
    0x57, 0x24, 0xb8,

    /* U+00EF "ï" */
    0xa3, 0x24, 0xb8,

    /* U+00F0 "ð" */
    0x62, 0x69, 0x99, 0x60,

    /* U+00F1 "ñ" */
    0xdb, 0xdb, 0x68,

    /* U+00F2 "ò" */
    0x44, 0x26, 0x99, 0x96,

    /* U+00F3 "ó" */
    0x22, 0x46, 0x99, 0x96,

    /* U+00F4 "ô" */
    0x6, 0x6, 0x99, 0x96,

    /* U+00F5 "õ" */
    0x60, 0x69, 0x99, 0x60,

    /* U+00F6 "ö" */
    0x60, 0x69, 0x99, 0x60,

    /* U+00F7 "÷" */
    0x20, 0x3e, 0x40,

    /* U+00F8 "ø" */
    0x79, 0xf9, 0xe0,

    /* U+00F9 "ù" */
    0x8a, 0xdb, 0x78,

    /* U+00FA "ú" */
    0x4a, 0xdb, 0x78,

    /* U+00FB "û" */
    0x42, 0xdb, 0x78,

    /* U+00FC "ü" */
    0xc2, 0xdb, 0x78,

    /* U+00FD "ý" */
    0x20, 0x24, 0xa5, 0x30, 0x84, 0x40,

    /* U+00FE "þ" */
    0x88, 0xe9, 0x99, 0xe8, 0x80,

    /* U+00FF "ÿ" */
    0x50, 0x24, 0xa5, 0x30, 0x84, 0x40,

    /* U+2014 "—" */
    0xf8,

    /* U+2022 "•" */
    0xf0,

    /* U+2026 "…" */
    0xa8,

    /* U+2190 "←" */
    0xf, 0x0,

    /* U+2191 "↑" */
    0x49, 0x20,

    /* U+2192 "→" */
    0xf, 0x0,

    /* U+2193 "↓" */
    0x49, 0x20,

    /* U+2194 "↔" */
    0xf, 0x0,

    /* U+2195 "↕" */
    0x49, 0x20,

    /* U+2261 "≡" */
    0xff, 0xf0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 77, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 77, .box_w = 1, .box_h = 7, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 2, .adv_w = 77, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 3, .adv_w = 77, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 8, .adv_w = 77, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 13, .adv_w = 77, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 17, .adv_w = 77, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 22, .adv_w = 77, .box_w = 1, .box_h = 2, .ofs_x = 2, .ofs_y = 5},
    {.bitmap_index = 23, .adv_w = 77, .box_w = 2, .box_h = 8, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 25, .adv_w = 77, .box_w = 2, .box_h = 8, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 27, .adv_w = 77, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 30, .adv_w = 77, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 33, .adv_w = 77, .box_w = 1, .box_h = 2, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 34, .adv_w = 77, .box_w = 2, .box_h = 1, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 35, .adv_w = 77, .box_w = 1, .box_h = 1, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 36, .adv_w = 77, .box_w = 4, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 40, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 44, .adv_w = 77, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 47, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 51, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 55, .adv_w = 77, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 60, .adv_w = 77, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 63, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 67, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 71, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 75, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 79, .adv_w = 77, .box_w = 1, .box_h = 4, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 80, .adv_w = 77, .box_w = 1, .box_h = 6, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 81, .adv_w = 77, .box_w = 4, .box_h = 4, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 83, .adv_w = 77, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 85, .adv_w = 77, .box_w = 4, .box_h = 4, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 87, .adv_w = 77, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 90, .adv_w = 77, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 95, .adv_w = 77, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 100, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 104, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 108, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 112, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 116, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 120, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 124, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 128, .adv_w = 77, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 131, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 135, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 139, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 143, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 147, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 151, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 155, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 159, .adv_w = 77, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 163, .adv_w = 77, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 168, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 172, .adv_w = 77, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 177, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 181, .adv_w = 77, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 186, .adv_w = 77, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 191, .adv_w = 77, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 196, .adv_w = 77, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 201, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 205, .adv_w = 77, .box_w = 2, .box_h = 9, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 208, .adv_w = 77, .box_w = 4, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 212, .adv_w = 77, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 215, .adv_w = 77, .box_w = 5, .box_h = 3, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 217, .adv_w = 77, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 218, .adv_w = 77, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 219, .adv_w = 77, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 222, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 226, .adv_w = 77, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 229, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 233, .adv_w = 77, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 236, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 240, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 244, .adv_w = 77, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 247, .adv_w = 77, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 250, .adv_w = 77, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 254, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 258, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 262, .adv_w = 77, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 266, .adv_w = 77, .box_w = 3, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 268, .adv_w = 77, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 271, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 275, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 279, .adv_w = 77, .box_w = 3, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 281, .adv_w = 77, .box_w = 3, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 283, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 287, .adv_w = 77, .box_w = 3, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 289, .adv_w = 77, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 292, .adv_w = 77, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 296, .adv_w = 77, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 300, .adv_w = 77, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 305, .adv_w = 77, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 308, .adv_w = 77, .box_w = 3, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 312, .adv_w = 77, .box_w = 1, .box_h = 9, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 314, .adv_w = 77, .box_w = 3, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 318, .adv_w = 77, .box_w = 4, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 319, .adv_w = 77, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 320, .adv_w = 77, .box_w = 1, .box_h = 6, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 321, .adv_w = 77, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 325, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 329, .adv_w = 77, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 331, .adv_w = 77, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 336, .adv_w = 77, .box_w = 1, .box_h = 8, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 337, .adv_w = 77, .box_w = 3, .box_h = 8, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 340, .adv_w = 77, .box_w = 3, .box_h = 1, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 341, .adv_w = 77, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 345, .adv_w = 77, .box_w = 3, .box_h = 4, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 347, .adv_w = 77, .box_w = 4, .box_h = 4, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 349, .adv_w = 77, .box_w = 4, .box_h = 3, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 351, .adv_w = 77, .box_w = 2, .box_h = 1, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 352, .adv_w = 77, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 356, .adv_w = 77, .box_w = 3, .box_h = 1, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 357, .adv_w = 77, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 359, .adv_w = 77, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 363, .adv_w = 77, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 364, .adv_w = 77, .box_w = 2, .box_h = 3, .ofs_x = 2, .ofs_y = 3},
    {.bitmap_index = 365, .adv_w = 77, .box_w = 2, .box_h = 2, .ofs_x = 2, .ofs_y = 6},
    {.bitmap_index = 366, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 370, .adv_w = 77, .box_w = 4, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 374, .adv_w = 77, .box_w = 1, .box_h = 1, .ofs_x = 2, .ofs_y = 3},
    {.bitmap_index = 375, .adv_w = 77, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 376, .adv_w = 77, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 378, .adv_w = 77, .box_w = 3, .box_h = 4, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 380, .adv_w = 77, .box_w = 4, .box_h = 4, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 382, .adv_w = 77, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 388, .adv_w = 77, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 393, .adv_w = 77, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 399, .adv_w = 77, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 402, .adv_w = 77, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 408, .adv_w = 77, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 414, .adv_w = 77, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 420, .adv_w = 77, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 425, .adv_w = 77, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 430, .adv_w = 77, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 435, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 439, .adv_w = 77, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 444, .adv_w = 77, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 449, .adv_w = 77, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 454, .adv_w = 77, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 459, .adv_w = 77, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 463, .adv_w = 77, .box_w = 3, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 467, .adv_w = 77, .box_w = 3, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 471, .adv_w = 77, .box_w = 3, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 475, .adv_w = 77, .box_w = 3, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 478, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 482, .adv_w = 77, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 486, .adv_w = 77, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 491, .adv_w = 77, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 496, .adv_w = 77, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 501, .adv_w = 77, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 506, .adv_w = 77, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 510, .adv_w = 77, .box_w = 3, .box_h = 4, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 512, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 516, .adv_w = 77, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 521, .adv_w = 77, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 526, .adv_w = 77, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 531, .adv_w = 77, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 535, .adv_w = 77, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 541, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 545, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 549, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 553, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 557, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 561, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 565, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 569, .adv_w = 77, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 573, .adv_w = 77, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 577, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 581, .adv_w = 77, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 585, .adv_w = 77, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 589, .adv_w = 77, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 593, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 597, .adv_w = 77, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 600, .adv_w = 77, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 603, .adv_w = 77, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 606, .adv_w = 77, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 609, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 613, .adv_w = 77, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 616, .adv_w = 77, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 620, .adv_w = 77, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 624, .adv_w = 77, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 628, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 632, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 636, .adv_w = 77, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 639, .adv_w = 77, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 642, .adv_w = 77, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 645, .adv_w = 77, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 648, .adv_w = 77, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 651, .adv_w = 77, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 654, .adv_w = 77, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 660, .adv_w = 77, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 665, .adv_w = 77, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 671, .adv_w = 77, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 672, .adv_w = 77, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 673, .adv_w = 77, .box_w = 5, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 674, .adv_w = 77, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 676, .adv_w = 77, .box_w = 3, .box_h = 4, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 678, .adv_w = 77, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 680, .adv_w = 77, .box_w = 3, .box_h = 4, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 682, .adv_w = 77, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 684, .adv_w = 77, .box_w = 3, .box_h = 4, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 686, .adv_w = 77, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_2[] = {
    0x0, 0xe, 0x12, 0x17c, 0x17d, 0x17e, 0x17f, 0x180,
    0x181, 0x24d
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 160, .range_length = 96, .glyph_id_start = 96,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 8212, .range_length = 590, .glyph_id_start = 192,
        .unicode_list = unicode_list_2, .glyph_id_ofs_list = NULL, .list_length = 10, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LV_VERSION_CHECK(8, 0, 0)
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 3,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LV_VERSION_CHECK(8, 0, 0)
    .cache = &cache
#endif
};


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if 1
const lv_font_t phos_mono_13_cs = {
#else
lv_font_t phos_mono_13_cs = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 11,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc           /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
};



#endif /*#if PHOS_MONO_13_CS*/

