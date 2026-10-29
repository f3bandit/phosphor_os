/*******************************************************************************
 * Size: 14 px
 * Bpp: 1
 * Opts: --no-compress --no-prefilter --bpp 1 --size 14 --font /usr/share/fonts/opentype/inter/Inter-Bold.otf -r 0x20-0x7E,0xA0-0xFF,0x2013-0x2014,0x2018-0x201D,0x2022,0x2026,0x203A,0x2190-0x2193,0x2713 --format lvgl --lv-include lvgl.h -o fonts/phos_inter_semi_22_cs.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef PHOS_INTER_SEMI_22_CS
#define PHOS_INTER_SEMI_22_CS 1
#endif

#if PHOS_INTER_SEMI_22_CS

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xfc, 0xc,

    /* U+0022 "\"" */
    0xde, 0xf7, 0xb0,

    /* U+0023 "#" */
    0x13, 0x9, 0x8c, 0xcf, 0xf3, 0x21, 0x90, 0x99,
    0xfe, 0x66, 0x32, 0x19, 0x0,

    /* U+0024 "$" */
    0x10, 0x7c, 0xff, 0xd3, 0xd0, 0xf0, 0x7e, 0x1f,
    0x13, 0xd3, 0xff, 0x7c, 0x10,

    /* U+0025 "%" */
    0x70, 0x4d, 0x88, 0xd9, 0xd, 0x90, 0x72, 0x0,
    0x40, 0x4, 0xe0, 0x9b, 0x9, 0xb1, 0x1b, 0x20,
    0xe0,

    /* U+0026 "&" */
    0x38, 0x36, 0x1b, 0xf, 0x87, 0x81, 0x81, 0xcd,
    0x96, 0xcf, 0x63, 0x1f, 0xc0,

    /* U+0027 "'" */
    0xff,

    /* U+0028 "(" */
    0x66, 0x6c, 0xcc, 0xcc, 0xcc, 0x66, 0x60,

    /* U+0029 ")" */
    0x66, 0x63, 0x33, 0x33, 0x33, 0x66, 0x60,

    /* U+002A "*" */
    0x25, 0x5c, 0xea, 0x90,

    /* U+002B "+" */
    0x30, 0x60, 0xc7, 0xf3, 0x6, 0xc, 0x0,

    /* U+002C "," */
    0xfe,

    /* U+002D "-" */
    0xf8,

    /* U+002E "." */
    0xc0,

    /* U+002F "/" */
    0x18, 0xc6, 0x23, 0x18, 0xc4, 0x63, 0x18, 0x8c,
    0x0,

    /* U+0030 "0" */
    0x3c, 0x66, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0x66, 0x3c,

    /* U+0031 "1" */
    0x3b, 0xf7, 0x31, 0x8c, 0x63, 0x18, 0xc6,

    /* U+0032 "2" */
    0x79, 0x8b, 0x18, 0x30, 0xe1, 0x87, 0x1c, 0x70,
    0xc3, 0xf8,

    /* U+0033 "3" */
    0x7d, 0x8f, 0x18, 0x30, 0xe7, 0x3, 0x83, 0xc7,
    0x8d, 0xf0,

    /* U+0034 "4" */
    0xe, 0xe, 0x1e, 0x3e, 0x36, 0x66, 0x66, 0xc6,
    0xff, 0x6, 0x6,

    /* U+0035 "5" */
    0x7c, 0x83, 0x6, 0xf, 0xd9, 0xc1, 0x83, 0xc7,
    0x99, 0xe0,

    /* U+0036 "6" */
    0x3c, 0x67, 0x40, 0xc0, 0xdc, 0xe6, 0xc3, 0xc3,
    0xc3, 0x66, 0x3c,

    /* U+0037 "7" */
    0xfe, 0xc, 0x30, 0x61, 0xc3, 0x6, 0x18, 0x30,
    0xc1, 0x80,

    /* U+0038 "8" */
    0x3c, 0xe7, 0xc3, 0xc3, 0x66, 0x3c, 0xe7, 0xc3,
    0xc3, 0xe7, 0x3c,

    /* U+0039 "9" */
    0x3c, 0x66, 0xc3, 0xc3, 0xc3, 0xe7, 0x7b, 0x3,
    0x2, 0xe6, 0x3c,

    /* U+003A ":" */
    0xc0, 0xc,

    /* U+003B ";" */
    0xc0, 0xe, 0xa0,

    /* U+003C "<" */
    0x6, 0x3d, 0xe7, 0xf, 0x7, 0x83, 0x81,

    /* U+003D "=" */
    0xfe, 0x0, 0x0, 0xf, 0xe0,

    /* U+003E ">" */
    0xc1, 0xe0, 0xf0, 0x71, 0xef, 0x38, 0x40,

    /* U+003F "?" */
    0x79, 0x8f, 0x18, 0x30, 0xe3, 0x8e, 0x18, 0x30,
    0x0, 0xc0,

    /* U+0040 "@" */
    0xf, 0x81, 0xc3, 0x18, 0xc, 0xc0, 0x6c, 0xfd,
    0xee, 0x6f, 0x63, 0x7b, 0x1b, 0xd8, 0xde, 0xee,
    0x9b, 0xdc, 0xc0, 0x3, 0x84, 0x7, 0xe0,

    /* U+0041 "A" */
    0xe, 0x7, 0x81, 0xe0, 0x6c, 0x3b, 0xc, 0xc3,
    0x39, 0xfe, 0x61, 0x98, 0x7e, 0xc,

    /* U+0042 "B" */
    0xfc, 0xc6, 0xc6, 0xc6, 0xc6, 0xfc, 0xc7, 0xc3,
    0xc3, 0xc7, 0xfe,

    /* U+0043 "C" */
    0x1e, 0x31, 0x98, 0x78, 0xc, 0x6, 0x3, 0x1,
    0x80, 0x61, 0xb1, 0x87, 0x80,

    /* U+0044 "D" */
    0xfc, 0x63, 0xb0, 0xd8, 0x3c, 0x1e, 0xf, 0x7,
    0x83, 0xc3, 0x63, 0xbf, 0x0,

    /* U+0045 "E" */
    0xff, 0x83, 0x6, 0xc, 0x1f, 0xf0, 0x60, 0xc1,
    0x83, 0xf8,

    /* U+0046 "F" */
    0xff, 0x83, 0x6, 0xc, 0x18, 0x3f, 0xe0, 0xc1,
    0x83, 0x0,

    /* U+0047 "G" */
    0x1e, 0x31, 0x98, 0x78, 0x3c, 0x6, 0x3, 0x1f,
    0x83, 0x61, 0xb1, 0x87, 0x80,

    /* U+0048 "H" */
    0xc1, 0xe0, 0xf0, 0x78, 0x3c, 0x1f, 0xff, 0x7,
    0x83, 0xc1, 0xe0, 0xf0, 0x60,

    /* U+0049 "I" */
    0xff, 0xff, 0xfc,

    /* U+004A "J" */
    0x6, 0xc, 0x18, 0x30, 0x60, 0xc1, 0x83, 0xc7,
    0x8d, 0xf0,

    /* U+004B "K" */
    0xc3, 0x63, 0x33, 0x9b, 0x8d, 0x87, 0xc3, 0xf1,
    0x9c, 0xc6, 0x63, 0xb0, 0xc0,

    /* U+004C "L" */
    0xc1, 0x83, 0x6, 0xc, 0x18, 0x30, 0x60, 0xc1,
    0x83, 0xf8,

    /* U+004D "M" */
    0xe0, 0xfe, 0x3f, 0xc7, 0xf8, 0xfd, 0xb7, 0xb6,
    0xf6, 0xde, 0x53, 0xce, 0x79, 0xcf, 0x39, 0x80,

    /* U+004E "N" */
    0xe1, 0xf0, 0xfc, 0x7e, 0x3d, 0x9e, 0xef, 0x37,
    0x8f, 0xc7, 0xe1, 0xf0, 0xe0,

    /* U+004F "O" */
    0x3e, 0x31, 0x98, 0xd8, 0x3c, 0x1e, 0xf, 0x7,
    0x83, 0x63, 0x31, 0x8f, 0x80,

    /* U+0050 "P" */
    0xfc, 0xc6, 0xc3, 0xc3, 0xc3, 0xc6, 0xfc, 0xc0,
    0xc0, 0xc0, 0xc0,

    /* U+0051 "Q" */
    0x3c, 0x31, 0x98, 0xd8, 0x3c, 0x1e, 0xf, 0x7,
    0x83, 0x6f, 0xb3, 0x8f, 0xc0, 0x30,

    /* U+0052 "R" */
    0xfc, 0xc6, 0xc3, 0xc3, 0xc3, 0xc7, 0xfe, 0xce,
    0xc6, 0xc7, 0xc3,

    /* U+0053 "S" */
    0x3c, 0x47, 0xc3, 0xe0, 0xfc, 0x7e, 0x3f, 0x7,
    0xc3, 0xe2, 0x7c,

    /* U+0054 "T" */
    0xff, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x18,

    /* U+0055 "U" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0x66, 0x3c,

    /* U+0056 "V" */
    0xe0, 0xd8, 0x76, 0x19, 0xc6, 0x33, 0x8c, 0xc3,
    0xb0, 0x6c, 0x1e, 0x7, 0x80, 0xe0,

    /* U+0057 "W" */
    0xe3, 0xd, 0x8e, 0x36, 0x39, 0xd9, 0xe6, 0x77,
    0x98, 0xdb, 0x63, 0x6d, 0x8d, 0x3c, 0x3c, 0xf0,
    0x71, 0xc1, 0xc7, 0x0,

    /* U+0058 "X" */
    0x61, 0x8c, 0xe3, 0xb0, 0x7c, 0x1e, 0x3, 0x81,
    0xe0, 0x7c, 0x33, 0x1c, 0x66, 0x1c,

    /* U+0059 "Y" */
    0xe1, 0x98, 0x63, 0x30, 0xcc, 0x1e, 0x7, 0x80,
    0xc0, 0x30, 0xc, 0x3, 0x0, 0xc0,

    /* U+005A "Z" */
    0xff, 0x7, 0xe, 0xc, 0x1c, 0x18, 0x38, 0x30,
    0x70, 0xe0, 0xff,

    /* U+005B "[" */
    0xfc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xf0,

    /* U+005C "\\" */
    0xc6, 0x18, 0xc6, 0x30, 0xc6, 0x30, 0x86, 0x31,
    0x80,

    /* U+005D "]" */
    0xf3, 0x33, 0x33, 0x33, 0x33, 0x33, 0xf0,

    /* U+005E "^" */
    0x10, 0xe2, 0x9b, 0x44,

    /* U+005F "_" */
    0xfe,

    /* U+0060 "`" */
    0x40,

    /* U+0061 "a" */
    0x7d, 0x8c, 0x3b, 0xff, 0x78, 0xf3, 0xbf,

    /* U+0062 "b" */
    0xc1, 0x83, 0x7, 0xee, 0xd8, 0xf1, 0xe3, 0xc7,
    0xdb, 0xf0,

    /* U+0063 "c" */
    0x3c, 0xcf, 0x16, 0xc, 0x18, 0x99, 0x9e,

    /* U+0064 "d" */
    0x6, 0xc, 0x1b, 0xf6, 0xf8, 0xf1, 0xe3, 0xc6,
    0xdd, 0xf8,

    /* U+0065 "e" */
    0x3c, 0x66, 0xc6, 0xff, 0xc0, 0xc0, 0x66, 0x3c,

    /* U+0066 "f" */
    0x13, 0xd8, 0xcf, 0xb1, 0x8c, 0x63, 0x18, 0xc0,

    /* U+0067 "g" */
    0x7e, 0xdf, 0x1e, 0x3c, 0x78, 0xfb, 0xbf, 0x7,
    0x8d, 0xf0,

    /* U+0068 "h" */
    0xc1, 0x83, 0x7, 0xec, 0x78, 0xf1, 0xe3, 0xc7,
    0x8f, 0x18,

    /* U+0069 "i" */
    0x30, 0xff, 0xff,

    /* U+006A "j" */
    0xc, 0x6, 0xdb, 0x6d, 0xb6, 0xf0,

    /* U+006B "k" */
    0xc1, 0x83, 0x6, 0x7d, 0xdf, 0x3c, 0x7c, 0xdd,
    0x9b, 0x38,

    /* U+006C "l" */
    0xff, 0xff, 0xfc,

    /* U+006D "m" */
    0xfd, 0xec, 0x63, 0xc6, 0x3c, 0x63, 0xc6, 0x3c,
    0x63, 0xc6, 0x3c, 0x63,

    /* U+006E "n" */
    0xfd, 0x8f, 0x1e, 0x3c, 0x78, 0xf1, 0xe3,

    /* U+006F "o" */
    0x38, 0xdb, 0x1e, 0x3c, 0x78, 0xdb, 0x1c,

    /* U+0070 "p" */
    0xfd, 0xdb, 0x1e, 0x3c, 0x78, 0xfb, 0x7e, 0xc1,
    0x83, 0x0,

    /* U+0071 "q" */
    0x7e, 0xdf, 0x1e, 0x3c, 0x78, 0xdb, 0xbf, 0x6,
    0xc, 0x18,

    /* U+0072 "r" */
    0xff, 0x31, 0x8c, 0x63, 0x18,

    /* U+0073 "s" */
    0x7d, 0x8f, 0x87, 0xe7, 0xe1, 0xf1, 0xbe,

    /* U+0074 "t" */
    0x63, 0x3e, 0xc6, 0x31, 0x8c, 0x61, 0xc0,

    /* U+0075 "u" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x78, 0xf1, 0xbf,

    /* U+0076 "v" */
    0xe3, 0x63, 0x66, 0x66, 0x36, 0x3c, 0x3c, 0x1c,

    /* U+0077 "w" */
    0xc6, 0x76, 0x66, 0x6e, 0x66, 0xf6, 0x6b, 0x43,
    0x9c, 0x39, 0xc3, 0x9c,

    /* U+0078 "x" */
    0x66, 0x66, 0x3c, 0x1c, 0x3c, 0x3c, 0x66, 0x67,

    /* U+0079 "y" */
    0xe3, 0x63, 0x66, 0x66, 0x36, 0x3c, 0x1c, 0x18,
    0x18, 0x78, 0x70,

    /* U+007A "z" */
    0xfc, 0x71, 0x8c, 0x31, 0x8e, 0x3f,

    /* U+007B "{" */
    0x4, 0xf3, 0xc, 0x30, 0xcc, 0xc, 0x30, 0xc3,
    0xc, 0x1c,

    /* U+007C "|" */
    0xff, 0xff, 0xff, 0xff, 0xc0,

    /* U+007D "}" */
    0x83, 0xc3, 0xc, 0x30, 0xc0, 0xcc, 0x30, 0xc3,
    0xc, 0xe0,

    /* U+007E "~" */
    0x73, 0xce,

    /* U+00A0 " " */
    0x0,

    /* U+00A1 "¡" */
    0xcf, 0xff, 0xc0,

    /* U+00A2 "¢" */
    0x10, 0x71, 0xf6, 0xbd, 0x1a, 0x34, 0x6b, 0x7c,
    0x70, 0x40,

    /* U+00A3 "£" */
    0x1c, 0x32, 0x62, 0x60, 0x60, 0xfc, 0x60, 0x60,
    0x60, 0x62, 0xfe,

    /* U+00A4 "¤" */
    0xbd, 0xa1, 0xb0, 0xd0, 0x38, 0x1c, 0xf, 0xd,
    0x86, 0xbd, 0x80,

    /* U+00A5 "¥" */
    0x67, 0x66, 0x66, 0x3c, 0x3c, 0x3c, 0x7e, 0x18,
    0x7e, 0x18, 0x18,

    /* U+00A6 "¦" */
    0xff, 0xc0, 0xff, 0xc0,

    /* U+00A7 "§" */
    0x79, 0x9b, 0x3, 0x8f, 0xdf, 0xb3, 0x72, 0x38,
    0x1b, 0x33, 0xc0,

    /* U+00A8 "¨" */
    0xd8,

    /* U+00A9 "©" */
    0x1f, 0x6, 0x31, 0x83, 0x6f, 0x3f, 0x37, 0xe0,
    0xfc, 0xde, 0xf3, 0x60, 0xc6, 0x30, 0x7c, 0x0,

    /* U+00AA "ª" */
    0x69, 0x1f, 0x9f,

    /* U+00AB "«" */
    0x36, 0x6c, 0xd8, 0xd8, 0x6c, 0x36,

    /* U+00AC "¬" */
    0xfe, 0xc, 0x18,

    /* U+00AE "®" */
    0x3c, 0x42, 0x9d, 0x95, 0x9d, 0x95, 0x42, 0x3c,

    /* U+00AF "¯" */
    0xf0,

    /* U+00B0 "°" */
    0x76, 0xf7, 0xb7, 0x0,

    /* U+00B1 "±" */
    0x30, 0x63, 0xf9, 0x83, 0x0, 0x3f, 0x80,

    /* U+00B2 "²" */
    0x74, 0xc6, 0x66, 0x7c,

    /* U+00B3 "³" */
    0x79, 0x33, 0x3, 0xcd, 0xe0,

    /* U+00B4 "´" */
    0x40,

    /* U+00B5 "µ" */
    0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xff,
    0xc0, 0xc0, 0xc0,

    /* U+00B6 "¶" */
    0x7f, 0xef, 0xdf, 0xbf, 0x6e, 0xc5, 0x83, 0x6,
    0xc, 0x18,

    /* U+00B7 "·" */
    0xc0,

    /* U+00B8 "¸" */
    0x47, 0x80,

    /* U+00B9 "¹" */
    0x7f, 0x33, 0x33,

    /* U+00BA "º" */
    0x74, 0x63, 0x18, 0xb8,

    /* U+00BB "»" */
    0xd8, 0x6c, 0x36, 0x36, 0x6c, 0xd8,

    /* U+00BC "¼" */
    0x70, 0x8f, 0x18, 0x31, 0x3, 0x30, 0x36, 0x3,
    0x6e, 0xc, 0xe0, 0x8e, 0x19, 0x63, 0x1f, 0x30,
    0x60,

    /* U+00BD "½" */
    0x70, 0x8b, 0x8, 0x31, 0x3, 0x20, 0x36, 0xe0,
    0x4b, 0xc, 0x31, 0x86, 0x10, 0xc3, 0xf,

    /* U+00BE "¾" */
    0x70, 0x45, 0x8c, 0x19, 0x82, 0x10, 0x1b, 0x7,
    0x67, 0x4, 0x70, 0xcb, 0x9, 0xb1, 0x9f, 0x30,
    0x30,

    /* U+00BF "¿" */
    0x18, 0x0, 0x60, 0xc3, 0x8e, 0x38, 0x60, 0xc7,
    0x8c, 0xf0,

    /* U+00C0 "À" */
    0x8, 0x0, 0x0, 0xe0, 0x78, 0x1e, 0x6, 0xc3,
    0xb0, 0xcc, 0x33, 0x9f, 0xe6, 0x19, 0x87, 0xe0,
    0xc0,

    /* U+00C1 "Á" */
    0x6, 0x0, 0x0, 0xe0, 0x78, 0x1e, 0x6, 0xc3,
    0xb0, 0xcc, 0x33, 0x9f, 0xe6, 0x19, 0x87, 0xe0,
    0xc0,

    /* U+00C2 "Â" */
    0xc, 0x7, 0x83, 0x30, 0x38, 0x1e, 0x7, 0x81,
    0xb0, 0xec, 0x33, 0xc, 0xe7, 0xf9, 0x86, 0x61,
    0xf8, 0x30,

    /* U+00C3 "Ã" */
    0x1e, 0x0, 0x0, 0xe0, 0x78, 0x1e, 0x6, 0xc1,
    0xb0, 0xcc, 0x33, 0x8f, 0xe6, 0x19, 0x87, 0x60,
    0xc0,

    /* U+00C4 "Ä" */
    0x36, 0x0, 0x7, 0x3, 0x83, 0x61, 0xb0, 0xd8,
    0xee, 0x63, 0x3f, 0xb8, 0xf8, 0x3c, 0x18,

    /* U+00C5 "Å" */
    0x1e, 0x4, 0x81, 0xe0, 0x78, 0x1e, 0x7, 0x83,
    0xf0, 0xcc, 0x33, 0x1c, 0xe7, 0xf9, 0x86, 0xe1,
    0xf0, 0x30,

    /* U+00C6 "Æ" */
    0xf, 0xf8, 0xd8, 0x6, 0xc0, 0x36, 0x3, 0x30,
    0x19, 0xfd, 0xcc, 0xf, 0xe0, 0x63, 0x6, 0x18,
    0x30, 0xfe,

    /* U+00C7 "Ç" */
    0x1e, 0x31, 0x98, 0x78, 0xc, 0x6, 0x3, 0x1,
    0x80, 0x61, 0xb1, 0x8f, 0x81, 0x0, 0x40, 0x60,

    /* U+00C8 "È" */
    0x30, 0x3, 0xfe, 0xc, 0x18, 0x30, 0x7f, 0xc1,
    0x83, 0x6, 0xf, 0xe0,

    /* U+00C9 "É" */
    0x18, 0x3, 0xfe, 0xc, 0x18, 0x30, 0x7f, 0xc1,
    0x83, 0x6, 0xf, 0xe0,

    /* U+00CA "Ê" */
    0x10, 0x71, 0x37, 0xfc, 0x18, 0x30, 0x60, 0xff,
    0x83, 0x6, 0xc, 0x1f, 0xc0,

    /* U+00CB "Ë" */
    0x6c, 0x3, 0xfe, 0xc, 0x18, 0x30, 0x7f, 0xc1,
    0x83, 0x6, 0xf, 0xe0,

    /* U+00CC "Ì" */
    0x41, 0xb6, 0xdb, 0x6d, 0xb6,

    /* U+00CD "Í" */
    0x43, 0x6d, 0xb6, 0xdb, 0x6c,

    /* U+00CE "Î" */
    0x31, 0xec, 0xcc, 0x30, 0xc3, 0xc, 0x30, 0xc3,
    0xc, 0x30, 0xc0,

    /* U+00CF "Ï" */
    0xd8, 0xc, 0x63, 0x18, 0xc6, 0x31, 0x8c, 0x63,
    0x0,

    /* U+00D0 "Ð" */
    0x7e, 0x18, 0xe6, 0x19, 0x83, 0x60, 0xff, 0x36,
    0xd, 0x83, 0x61, 0x98, 0xe7, 0xe0,

    /* U+00D1 "Ñ" */
    0x3e, 0x0, 0x38, 0x7c, 0x3f, 0x1f, 0x8f, 0x67,
    0xbb, 0xcd, 0xe3, 0xf1, 0xf8, 0x7c, 0x38,

    /* U+00D2 "Ò" */
    0x18, 0x0, 0xf, 0x8c, 0x66, 0x36, 0xf, 0x7,
    0x83, 0xc1, 0xe0, 0xd8, 0xcc, 0x63, 0xe0,

    /* U+00D3 "Ó" */
    0xc, 0x0, 0xf, 0x8c, 0x66, 0x36, 0xf, 0x7,
    0x83, 0xc1, 0xe0, 0xd8, 0xcc, 0x63, 0xe0,

    /* U+00D4 "Ô" */
    0x1c, 0xa, 0xd, 0x87, 0xc6, 0x33, 0x1b, 0x7,
    0x83, 0xc1, 0xe0, 0xf0, 0x6c, 0x66, 0x31, 0xf0,

    /* U+00D5 "Õ" */
    0x1a, 0x16, 0x0, 0x7, 0xc6, 0x33, 0x1b, 0x7,
    0x83, 0xc1, 0xe0, 0xf0, 0x6c, 0x66, 0x31, 0xf0,

    /* U+00D6 "Ö" */
    0x36, 0x0, 0xf, 0x8c, 0x66, 0x36, 0xf, 0x7,
    0x83, 0xc1, 0xe0, 0xd8, 0xcc, 0x63, 0xe0,

    /* U+00D7 "×" */
    0x43, 0xcd, 0xf1, 0xc3, 0xcd, 0xf1, 0x80,

    /* U+00D8 "Ø" */
    0x1f, 0x31, 0x91, 0xf8, 0xbc, 0xde, 0xcf, 0x67,
    0xe3, 0xf3, 0x31, 0x9f, 0x80,

    /* U+00D9 "Ù" */
    0x10, 0x0, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0x66, 0x3c,

    /* U+00DA "Ú" */
    0x8, 0x0, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0x66, 0x3c,

    /* U+00DB "Û" */
    0x18, 0x24, 0x0, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0x66, 0x3c,

    /* U+00DC "Ü" */
    0x36, 0x0, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0x66, 0x3c,

    /* U+00DD "Ý" */
    0x4, 0x0, 0xe, 0x19, 0x86, 0x33, 0xc, 0xc1,
    0xe0, 0x78, 0xc, 0x3, 0x0, 0xc0, 0x30, 0xc,
    0x0,

    /* U+00DE "Þ" */
    0xc1, 0x83, 0x7, 0xec, 0x78, 0xf1, 0xfe, 0xc1,
    0x83, 0x0,

    /* U+00DF "ß" */
    0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xdc, 0xc7, 0xc3,
    0xc3, 0xc7, 0xde,

    /* U+00E0 "à" */
    0x30, 0x0, 0x3, 0xec, 0x61, 0xdf, 0xfb, 0xc7,
    0x9d, 0xf8,

    /* U+00E1 "á" */
    0x18, 0x0, 0x3, 0xec, 0x61, 0xdf, 0xfb, 0xc7,
    0x9d, 0xf8,

    /* U+00E2 "â" */
    0x18, 0x71, 0xb0, 0x7, 0xd8, 0xc3, 0xbf, 0xf7,
    0x8f, 0x3b, 0xf0,

    /* U+00E3 "ã" */
    0x4, 0xf8, 0x0, 0x7, 0xd8, 0xc3, 0xbf, 0xf7,
    0x8f, 0x3b, 0xf0,

    /* U+00E4 "ä" */
    0x0, 0xd8, 0x0, 0x7, 0xdc, 0xc1, 0xbf, 0xf7,
    0x8f, 0x3b, 0xf0,

    /* U+00E5 "å" */
    0x38, 0x50, 0xe0, 0x7, 0xd8, 0xc1, 0xbf, 0xf7,
    0x8f, 0x3b, 0xf0,

    /* U+00E6 "æ" */
    0x7f, 0xcc, 0x72, 0x6, 0x37, 0xff, 0xc6, 0xc,
    0x60, 0xcf, 0x37, 0xbe,

    /* U+00E7 "ç" */
    0x3c, 0xcf, 0x6, 0xc, 0x1c, 0xdf, 0x9c, 0x18,
    0x18, 0x60,

    /* U+00E8 "è" */
    0x30, 0x0, 0x0, 0x3c, 0x66, 0xc6, 0xff, 0xc0,
    0xc0, 0x66, 0x3c,

    /* U+00E9 "é" */
    0x18, 0x0, 0x0, 0x3c, 0x66, 0xc6, 0xff, 0xc0,
    0xc0, 0x66, 0x3c,

    /* U+00EA "ê" */
    0x18, 0x38, 0x6c, 0x0, 0x3c, 0x66, 0xc6, 0xff,
    0xc0, 0xc0, 0x66, 0x3c,

    /* U+00EB "ë" */
    0x0, 0x6c, 0x0, 0x0, 0x3c, 0x66, 0xc6, 0xff,
    0xc0, 0xc0, 0x66, 0x3c,

    /* U+00EC "ì" */
    0x40, 0x36, 0xdb, 0x6d, 0x80,

    /* U+00ED "í" */
    0x40, 0x6d, 0xb6, 0xdb, 0x0,

    /* U+00EE "î" */
    0x31, 0x20, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xc3,
    0x0,

    /* U+00EF "ï" */
    0x6, 0xc0, 0x3, 0x18, 0xc6, 0x31, 0x8c, 0x60,

    /* U+00F0 "ð" */
    0x40, 0xf0, 0xe2, 0x67, 0x5d, 0xf1, 0xe3, 0xc6,
    0xd8, 0xe0,

    /* U+00F1 "ñ" */
    0x24, 0xf8, 0x0, 0xf, 0xd8, 0xf1, 0xe3, 0xc7,
    0x8f, 0x1e, 0x30,

    /* U+00F2 "ò" */
    0x30, 0x0, 0x1, 0xc6, 0xd8, 0xf1, 0xe3, 0xc6,
    0xd8, 0xe0,

    /* U+00F3 "ó" */
    0x18, 0x0, 0x1, 0xc6, 0xd8, 0xf1, 0xe3, 0xc6,
    0xd8, 0xe0,

    /* U+00F4 "ô" */
    0x10, 0x71, 0xb0, 0x3, 0x8d, 0xb1, 0xe3, 0xc7,
    0x8d, 0xb1, 0xc0,

    /* U+00F5 "õ" */
    0x24, 0xf0, 0x0, 0x3, 0x8d, 0xb1, 0xe3, 0xc7,
    0x8d, 0xb1, 0xc0,

    /* U+00F6 "ö" */
    0x0, 0xd8, 0x0, 0x3, 0x8d, 0xb1, 0xe3, 0xc7,
    0x8d, 0xb1, 0xc0,

    /* U+00F7 "÷" */
    0x30, 0x0, 0x7, 0xf0, 0x0, 0x0, 0x18,

    /* U+00F8 "ø" */
    0x3e, 0xdf, 0x3e, 0xbf, 0x7c, 0xfb, 0x7c,

    /* U+00F9 "ù" */
    0x30, 0x0, 0x6, 0x3c, 0x78, 0xf1, 0xe3, 0xc7,
    0x8d, 0xf8,

    /* U+00FA "ú" */
    0x18, 0x0, 0x6, 0x3c, 0x78, 0xf1, 0xe3, 0xc7,
    0x8d, 0xf8,

    /* U+00FB "û" */
    0x38, 0xd8, 0x6, 0x3c, 0x78, 0xf1, 0xe3, 0xc7,
    0x8d, 0xf8,

    /* U+00FC "ü" */
    0x0, 0xd8, 0x0, 0xc, 0x78, 0xf1, 0xe3, 0xc7,
    0x8f, 0x1b, 0xf0,

    /* U+00FD "ý" */
    0xc, 0x0, 0x0, 0xe3, 0x63, 0x66, 0x66, 0x36,
    0x3c, 0x1c, 0x18, 0x18, 0x78, 0x70,

    /* U+00FE "þ" */
    0xc1, 0x83, 0x7, 0xee, 0xd8, 0xf1, 0xe3, 0xc7,
    0xdb, 0xf6, 0xc, 0x18, 0x0,

    /* U+00FF "ÿ" */
    0x0, 0xd8, 0x0, 0xc, 0x78, 0xf1, 0xb6, 0x6c,
    0xd0, 0xe1, 0xc3, 0x1e, 0x38, 0x0,

    /* U+2013 "–" */
    0xfe,

    /* U+2014 "—" */
    0xff, 0xfc,

    /* U+2018 "‘" */
    0x6d, 0x60,

    /* U+2019 "’" */
    0x6b, 0x60,

    /* U+201A "‚" */
    0xfe,

    /* U+201B "‛" */
    0xfd,

    /* U+201C "“" */
    0x6d, 0xb4, 0xf6,

    /* U+201D "”" */
    0x6d, 0x2d, 0xb6,

    /* U+2022 "•" */
    0x6f, 0xf6,

    /* U+2026 "…" */
    0xc6, 0x30,

    /* U+203A "›" */
    0xc6, 0x33, 0x6c,

    /* U+2190 "←" */
    0x4, 0x1, 0x80, 0xe0, 0x38, 0xf, 0xfe, 0xe0,
    0x6, 0x0, 0x60, 0x4, 0x0,

    /* U+2191 "↑" */
    0xc, 0x7, 0x83, 0xf1, 0xb6, 0xcc, 0xc3, 0x0,
    0xc0, 0x30, 0xc, 0x3, 0x0,

    /* U+2192 "→" */
    0xc, 0x3, 0x80, 0x70, 0x6, 0xff, 0xc0, 0x60,
    0x70, 0x38, 0xc, 0x0,

    /* U+2193 "↓" */
    0xc, 0x3, 0x0, 0xc0, 0x30, 0xc, 0x33, 0x36,
    0xd8, 0xfc, 0x1e, 0x3, 0x0,

    /* U+2713 "✓" */
    0x0, 0xc0, 0x38, 0xe, 0x33, 0x87, 0xe0, 0x78,
    0x6, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 53, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 76, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 124, .box_w = 5, .box_h = 4, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 7, .adv_w = 145, .box_w = 9, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 20, .adv_w = 147, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 33, .adv_w = 228, .box_w = 12, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 50, .adv_w = 151, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 63, .adv_w = 76, .box_w = 2, .box_h = 4, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 64, .adv_w = 84, .box_w = 4, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 71, .adv_w = 84, .box_w = 4, .box_h = 13, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 78, .adv_w = 125, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 82, .adv_w = 152, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 89, .adv_w = 75, .box_w = 2, .box_h = 4, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 90, .adv_w = 105, .box_w = 5, .box_h = 1, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 91, .adv_w = 75, .box_w = 2, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 92, .adv_w = 87, .box_w = 5, .box_h = 13, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 101, .adv_w = 151, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 112, .adv_w = 97, .box_w = 5, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 119, .adv_w = 141, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 129, .adv_w = 145, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 139, .adv_w = 151, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 150, .adv_w = 143, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 160, .adv_w = 145, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 171, .adv_w = 130, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 181, .adv_w = 146, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 192, .adv_w = 145, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 203, .adv_w = 75, .box_w = 2, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 205, .adv_w = 77, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 208, .adv_w = 152, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 215, .adv_w = 152, .box_w = 7, .box_h = 5, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 220, .adv_w = 152, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 227, .adv_w = 125, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 237, .adv_w = 228, .box_w = 13, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 260, .adv_w = 167, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 274, .adv_w = 148, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 285, .adv_w = 166, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 298, .adv_w = 162, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 311, .adv_w = 136, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 321, .adv_w = 131, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 331, .adv_w = 168, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 344, .adv_w = 167, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 357, .adv_w = 63, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 360, .adv_w = 131, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 370, .adv_w = 161, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 383, .adv_w = 127, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 393, .adv_w = 209, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 409, .adv_w = 171, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 422, .adv_w = 173, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 435, .adv_w = 145, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 446, .adv_w = 174, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 460, .adv_w = 147, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 471, .adv_w = 147, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 482, .adv_w = 150, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 493, .adv_w = 164, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 504, .adv_w = 167, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 518, .adv_w = 232, .box_w = 14, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 538, .adv_w = 165, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 552, .adv_w = 164, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 566, .adv_w = 149, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 577, .adv_w = 84, .box_w = 4, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 584, .adv_w = 87, .box_w = 5, .box_h = 13, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 593, .adv_w = 84, .box_w = 4, .box_h = 13, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 600, .adv_w = 109, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 604, .adv_w = 107, .box_w = 7, .box_h = 1, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 605, .adv_w = 82, .box_w = 3, .box_h = 1, .ofs_x = 1, .ofs_y = 10},
    {.bitmap_index = 606, .adv_w = 130, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 613, .adv_w = 141, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 623, .adv_w = 132, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 630, .adv_w = 141, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 640, .adv_w = 133, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 648, .adv_w = 89, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 656, .adv_w = 142, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 666, .adv_w = 139, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 676, .adv_w = 61, .box_w = 2, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 679, .adv_w = 61, .box_w = 3, .box_h = 15, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 685, .adv_w = 130, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 695, .adv_w = 61, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 698, .adv_w = 204, .box_w = 12, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 710, .adv_w = 139, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 717, .adv_w = 137, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 724, .adv_w = 141, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 734, .adv_w = 141, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 744, .adv_w = 91, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 749, .adv_w = 125, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 756, .adv_w = 82, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 763, .adv_w = 139, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 770, .adv_w = 134, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 778, .adv_w = 190, .box_w = 12, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 790, .adv_w = 130, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 798, .adv_w = 135, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 809, .adv_w = 128, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 815, .adv_w = 105, .box_w = 6, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 825, .adv_w = 83, .box_w = 2, .box_h = 17, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 830, .adv_w = 105, .box_w = 6, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 840, .adv_w = 152, .box_w = 8, .box_h = 2, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 842, .adv_w = 53, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 843, .adv_w = 76, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 846, .adv_w = 132, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 856, .adv_w = 143, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 867, .adv_w = 171, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 878, .adv_w = 128, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 889, .adv_w = 76, .box_w = 2, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 893, .adv_w = 132, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 904, .adv_w = 139, .box_w = 5, .box_h = 1, .ofs_x = 2, .ofs_y = 10},
    {.bitmap_index = 905, .adv_w = 205, .box_w = 11, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 921, .adv_w = 108, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 924, .adv_w = 148, .box_w = 8, .box_h = 6, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 930, .adv_w = 152, .box_w = 7, .box_h = 3, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 933, .adv_w = 149, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 941, .adv_w = 98, .box_w = 4, .box_h = 1, .ofs_x = 1, .ofs_y = 10},
    {.bitmap_index = 942, .adv_w = 103, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 946, .adv_w = 152, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 953, .adv_w = 103, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 957, .adv_w = 106, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 962, .adv_w = 82, .box_w = 3, .box_h = 1, .ofs_x = 1, .ofs_y = 10},
    {.bitmap_index = 963, .adv_w = 141, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 974, .adv_w = 134, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 984, .adv_w = 75, .box_w = 2, .box_h = 1, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 985, .adv_w = 82, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 987, .adv_w = 75, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 990, .adv_w = 108, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 994, .adv_w = 148, .box_w = 8, .box_h = 6, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 1000, .adv_w = 189, .box_w = 12, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1017, .adv_w = 197, .box_w = 12, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1032, .adv_w = 206, .box_w = 12, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1049, .adv_w = 125, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1059, .adv_w = 167, .box_w = 10, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1076, .adv_w = 167, .box_w = 10, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1093, .adv_w = 167, .box_w = 10, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1111, .adv_w = 167, .box_w = 10, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1128, .adv_w = 167, .box_w = 9, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1143, .adv_w = 167, .box_w = 10, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1161, .adv_w = 229, .box_w = 13, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1179, .adv_w = 166, .box_w = 9, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1195, .adv_w = 136, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1207, .adv_w = 136, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1219, .adv_w = 136, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1232, .adv_w = 136, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1244, .adv_w = 63, .box_w = 3, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1249, .adv_w = 63, .box_w = 3, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1254, .adv_w = 63, .box_w = 6, .box_h = 14, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 1265, .adv_w = 63, .box_w = 5, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1274, .adv_w = 170, .box_w = 10, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1288, .adv_w = 171, .box_w = 9, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1303, .adv_w = 173, .box_w = 9, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1318, .adv_w = 173, .box_w = 9, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1333, .adv_w = 173, .box_w = 9, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1349, .adv_w = 173, .box_w = 9, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1365, .adv_w = 173, .box_w = 9, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1380, .adv_w = 152, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1387, .adv_w = 173, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1400, .adv_w = 164, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1413, .adv_w = 164, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1426, .adv_w = 164, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1440, .adv_w = 164, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1453, .adv_w = 164, .box_w = 10, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1470, .adv_w = 150, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1480, .adv_w = 147, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1491, .adv_w = 130, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1501, .adv_w = 130, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1511, .adv_w = 130, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1522, .adv_w = 130, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1533, .adv_w = 130, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1544, .adv_w = 130, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1555, .adv_w = 204, .box_w = 12, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1567, .adv_w = 132, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1577, .adv_w = 133, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1588, .adv_w = 133, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1599, .adv_w = 133, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1611, .adv_w = 133, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1623, .adv_w = 61, .box_w = 3, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1628, .adv_w = 61, .box_w = 3, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1633, .adv_w = 61, .box_w = 6, .box_h = 11, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 1642, .adv_w = 61, .box_w = 5, .box_h = 12, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 1650, .adv_w = 134, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1660, .adv_w = 139, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1671, .adv_w = 137, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1681, .adv_w = 137, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1691, .adv_w = 137, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1702, .adv_w = 137, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1713, .adv_w = 137, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1724, .adv_w = 152, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1731, .adv_w = 137, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1738, .adv_w = 139, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1748, .adv_w = 139, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1758, .adv_w = 139, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1768, .adv_w = 139, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1779, .adv_w = 135, .box_w = 8, .box_h = 14, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1793, .adv_w = 141, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1806, .adv_w = 135, .box_w = 7, .box_h = 15, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1820, .adv_w = 112, .box_w = 7, .box_h = 1, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 1821, .adv_w = 224, .box_w = 14, .box_h = 1, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 1823, .adv_w = 70, .box_w = 3, .box_h = 4, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 1825, .adv_w = 70, .box_w = 3, .box_h = 4, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 1827, .adv_w = 65, .box_w = 2, .box_h = 4, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1828, .adv_w = 70, .box_w = 2, .box_h = 4, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 1829, .adv_w = 121, .box_w = 6, .box_h = 4, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 1832, .adv_w = 119, .box_w = 6, .box_h = 4, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 1835, .adv_w = 106, .box_w = 4, .box_h = 4, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 1837, .adv_w = 224, .box_w = 12, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1839, .adv_w = 97, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 1842, .adv_w = 214, .box_w = 11, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1855, .adv_w = 203, .box_w = 10, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1868, .adv_w = 214, .box_w = 10, .box_h = 9, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 1880, .adv_w = 203, .box_w = 10, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1893, .adv_w = 198, .box_w = 11, .box_h = 7, .ofs_x = 1, .ofs_y = 1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_3[] = {
    0x0, 0x1, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa,
    0xf, 0x13, 0x27, 0x17d, 0x17e, 0x17f, 0x180, 0x700
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 160, .range_length = 13, .glyph_id_start = 96,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 174, .range_length = 82, .glyph_id_start = 109,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 8211, .range_length = 1793, .glyph_id_start = 191,
        .unicode_list = unicode_list_3, .glyph_id_ofs_list = NULL, .list_length = 16, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
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
    .cmap_num = 4,
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
const lv_font_t phos_inter_semi_22_cs = {
#else
lv_font_t phos_inter_semi_22_cs = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 17,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc           /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
};



#endif /*#if PHOS_INTER_SEMI_22_CS*/

