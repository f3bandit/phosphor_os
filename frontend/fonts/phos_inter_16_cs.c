/*******************************************************************************
 * Size: 12 px
 * Bpp: 1
 * Opts: --no-compress --no-prefilter --bpp 1 --size 12 --font /usr/share/fonts/opentype/inter/Inter-SemiBold.otf -r 0x20-0x7E,0xA0-0xFF,0x2013-0x2014,0x2018-0x201D,0x2022,0x2026,0x203A,0x2190-0x2193,0x2713 --format lvgl --lv-include lvgl.h -o fonts/phos_inter_16_cs.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef PHOS_INTER_16_CS
#define PHOS_INTER_16_CS 1
#endif

#if PHOS_INTER_16_CS

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xf3, 0xc0,

    /* U+0022 "\"" */
    0xff, 0xf0,

    /* U+0023 "#" */
    0x36, 0x24, 0x7e, 0x24, 0x24, 0xfe, 0x6c, 0x48,
    0x48,

    /* U+0024 "$" */
    0x10, 0xfb, 0x5e, 0x8f, 0x7, 0x85, 0xeb, 0xfe,
    0xf8, 0x40,

    /* U+0025 "%" */
    0x61, 0x24, 0xc9, 0x21, 0x90, 0xc, 0x6, 0x61,
    0x24, 0xc9, 0x21, 0x80,

    /* U+0026 "&" */
    0x78, 0xcc, 0xcc, 0x78, 0x30, 0xfa, 0xde, 0xce,
    0x7e,

    /* U+0027 "'" */
    0xfc,

    /* U+0028 "(" */
    0x2d, 0x6d, 0xb6, 0x4c, 0x80,

    /* U+0029 ")" */
    0x9b, 0x36, 0xdb, 0x5a, 0x0,

    /* U+002A "*" */
    0x27, 0xc9, 0xf2, 0x0,

    /* U+002B "+" */
    0x30, 0xcf, 0xcc, 0x30, 0xc0,

    /* U+002C "," */
    0xe8,

    /* U+002D "-" */
    0xf0,

    /* U+002E "." */
    0xf0,

    /* U+002F "/" */
    0x11, 0x32, 0x22, 0x64, 0x44, 0xc0,

    /* U+0030 "0" */
    0x38, 0xdb, 0x1e, 0x3c, 0x78, 0xf1, 0xb6, 0x38,

    /* U+0031 "1" */
    0x7f, 0xb3, 0x33, 0x33, 0x30,

    /* U+0032 "2" */
    0x7b, 0x3c, 0xc3, 0x18, 0xe7, 0x18, 0xfc,

    /* U+0033 "3" */
    0x7b, 0x30, 0xc3, 0x30, 0x30, 0xf3, 0x78,

    /* U+0034 "4" */
    0x1c, 0x38, 0xf1, 0x64, 0xd9, 0xbf, 0x86, 0xc,

    /* U+0035 "5" */
    0x79, 0xc, 0x3e, 0x4c, 0x30, 0xf3, 0x78,

    /* U+0036 "6" */
    0x3c, 0xcf, 0x6, 0xee, 0xf8, 0xf1, 0xb7, 0x3c,

    /* U+0037 "7" */
    0xfc, 0x30, 0x86, 0x10, 0xc2, 0x18, 0x40,

    /* U+0038 "8" */
    0x7b, 0x3c, 0xf3, 0x33, 0x3c, 0xf3, 0x78,

    /* U+0039 "9" */
    0x79, 0xdb, 0x1e, 0x3c, 0xef, 0xc1, 0xe6, 0x78,

    /* U+003A ":" */
    0xf0, 0x3c,

    /* U+003B ";" */
    0xf0, 0xe, 0x80,

    /* U+003C "<" */
    0x4, 0x7f, 0x30, 0x70, 0x70, 0x40,

    /* U+003D "=" */
    0xfc, 0x0, 0x3f,

    /* U+003E ">" */
    0x83, 0x83, 0xc3, 0x3b, 0x88, 0x0,

    /* U+003F "?" */
    0x7b, 0x30, 0xc7, 0x30, 0x80, 0x8, 0x20,

    /* U+0040 "@" */
    0x1f, 0x6, 0x39, 0x83, 0x67, 0xbd, 0x17, 0xa2,
    0xf4, 0x5e, 0x8b, 0xcf, 0xcc, 0x1, 0xc0, 0xf,
    0x80,

    /* U+0041 "A" */
    0x1c, 0x1c, 0x34, 0x36, 0x36, 0x66, 0x7f, 0x63,
    0xc1,

    /* U+0042 "B" */
    0xf9, 0x9b, 0x36, 0x6f, 0x18, 0xf1, 0xe3, 0xfc,

    /* U+0043 "C" */
    0x3c, 0x63, 0xc3, 0xc0, 0xc0, 0xc0, 0xc3, 0x63,
    0x3c,

    /* U+0044 "D" */
    0xf9, 0x9b, 0x1e, 0x3c, 0x78, 0xf1, 0xe6, 0xf8,

    /* U+0045 "E" */
    0xff, 0xc, 0x30, 0xff, 0xc, 0x30, 0xfc,

    /* U+0046 "F" */
    0xff, 0xc, 0x30, 0xff, 0xc, 0x30, 0xc0,

    /* U+0047 "G" */
    0x3c, 0x66, 0xc3, 0xc0, 0xcf, 0xc3, 0xc3, 0x66,
    0x3c,

    /* U+0048 "H" */
    0xc7, 0x8f, 0x1e, 0x3f, 0xf8, 0xf1, 0xe3, 0xc6,

    /* U+0049 "I" */
    0xff, 0xff, 0xc0,

    /* U+004A "J" */
    0xc, 0x30, 0xc3, 0xc, 0x3c, 0xf3, 0x78,

    /* U+004B "K" */
    0xc6, 0xcc, 0xc8, 0xd8, 0xf8, 0xe8, 0xcc, 0xc6,
    0xc6,

    /* U+004C "L" */
    0xc3, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xfc,

    /* U+004D "M" */
    0xe1, 0xf8, 0x7f, 0x3f, 0x4b, 0xd2, 0xf7, 0xbc,
    0xcf, 0x33, 0xcc, 0xc0,

    /* U+004E "N" */
    0xe3, 0xe3, 0xf3, 0xd3, 0xdb, 0xcb, 0xcf, 0xc7,
    0xc7,

    /* U+004F "O" */
    0x3c, 0x66, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x66,
    0x3c,

    /* U+0050 "P" */
    0xfd, 0x8f, 0x1e, 0x3c, 0x7f, 0xb0, 0x60, 0xc0,

    /* U+0051 "Q" */
    0x3c, 0x66, 0xc3, 0xc3, 0xc3, 0xc3, 0xcb, 0x6e,
    0x3c, 0x3,

    /* U+0052 "R" */
    0xfd, 0x8f, 0x1e, 0x3f, 0xd9, 0x33, 0x63, 0xc6,

    /* U+0053 "S" */
    0x7d, 0x8f, 0x7, 0x87, 0xc1, 0xf1, 0xe3, 0x7c,

    /* U+0054 "T" */
    0xfe, 0x60, 0xc1, 0x83, 0x6, 0xc, 0x18, 0x30,

    /* U+0055 "U" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x78, 0xf1, 0xe3, 0x3c,

    /* U+0056 "V" */
    0xc1, 0x63, 0x63, 0x62, 0x36, 0x36, 0x14, 0x1c,
    0x1c,

    /* U+0057 "W" */
    0xc6, 0x34, 0x63, 0x67, 0x26, 0xf6, 0x6d, 0x62,
    0x96, 0x29, 0x43, 0x9c, 0x18, 0xc0,

    /* U+0058 "X" */
    0x63, 0x26, 0x34, 0x1c, 0x18, 0x1c, 0x36, 0x66,
    0x43,

    /* U+0059 "Y" */
    0xc3, 0x66, 0x66, 0x3c, 0x3c, 0x18, 0x18, 0x18,
    0x18,

    /* U+005A "Z" */
    0xfc, 0x31, 0x84, 0x30, 0x86, 0x30, 0xfc,

    /* U+005B "[" */
    0xfb, 0x6d, 0xb6, 0xdb, 0x80,

    /* U+005C "\\" */
    0xc4, 0x44, 0x62, 0x22, 0x31, 0x10,

    /* U+005D "]" */
    0xed, 0xb6, 0xdb, 0x6f, 0x80,

    /* U+005E "^" */
    0x31, 0x96, 0x90,

    /* U+005F "_" */
    0xfc,

    /* U+0060 "`" */
    0xd0,

    /* U+0061 "a" */
    0x7b, 0x30, 0xdf, 0xcf, 0x37, 0xc0,

    /* U+0062 "b" */
    0xc3, 0xf, 0xb3, 0xcf, 0x3c, 0xf3, 0xf8,

    /* U+0063 "c" */
    0x7b, 0x9c, 0x30, 0xc3, 0x93, 0x80,

    /* U+0064 "d" */
    0xc, 0x37, 0xf3, 0xcf, 0x3c, 0xf3, 0x7c,

    /* U+0065 "e" */
    0x7b, 0x3c, 0x7f, 0xc3, 0x37, 0x80,

    /* U+0066 "f" */
    0x76, 0xf6, 0x66, 0x66, 0x60,

    /* U+0067 "g" */
    0x7f, 0x3c, 0xf3, 0xcf, 0x37, 0xc3, 0xcd, 0xe0,

    /* U+0068 "h" */
    0xc3, 0xf, 0xb3, 0xcf, 0x3c, 0xf3, 0xcc,

    /* U+0069 "i" */
    0xff, 0xff, 0xc0,

    /* U+006A "j" */
    0x6d, 0xb6, 0xdb, 0x6d, 0xe0,

    /* U+006B "k" */
    0xc3, 0xc, 0xf6, 0xf3, 0xcd, 0xb2, 0xcc,

    /* U+006C "l" */
    0xff, 0xff, 0xc0,

    /* U+006D "m" */
    0xfb, 0xb3, 0x3c, 0xcf, 0x33, 0xcc, 0xf3, 0x3c,
    0xcc,

    /* U+006E "n" */
    0xfb, 0x3c, 0xf3, 0xcf, 0x3c, 0xc0,

    /* U+006F "o" */
    0x7b, 0x3c, 0xf3, 0xcf, 0x37, 0x80,

    /* U+0070 "p" */
    0xfb, 0x3c, 0xf3, 0xcf, 0x3f, 0xb0, 0xc3, 0x0,

    /* U+0071 "q" */
    0x7f, 0x3c, 0xf3, 0xcf, 0x37, 0xc3, 0xc, 0x30,

    /* U+0072 "r" */
    0xfc, 0xcc, 0xcc, 0xc0,

    /* U+0073 "s" */
    0x7b, 0x3e, 0x1e, 0x1f, 0x37, 0x80,

    /* U+0074 "t" */
    0x66, 0xf6, 0x66, 0x66, 0x30,

    /* U+0075 "u" */
    0xcf, 0x3c, 0xf3, 0xcf, 0x37, 0xc0,

    /* U+0076 "v" */
    0xc6, 0x89, 0xb3, 0x62, 0x87, 0xe, 0x0,

    /* U+0077 "w" */
    0xcc, 0xd3, 0x24, 0xc9, 0xfe, 0x73, 0x8c, 0xc3,
    0x30,

    /* U+0078 "x" */
    0x4c, 0xd8, 0xe1, 0x83, 0x8d, 0x91, 0x0,

    /* U+0079 "y" */
    0xc6, 0x89, 0xb3, 0x62, 0x85, 0xe, 0x8, 0x30,
    0xc0,

    /* U+007A "z" */
    0xf8, 0xcc, 0x46, 0x63, 0xe0,

    /* U+007B "{" */
    0x19, 0x8c, 0x63, 0x60, 0xc6, 0x31, 0x86,

    /* U+007C "|" */
    0xff, 0xff, 0xff, 0xfc,

    /* U+007D "}" */
    0xc6, 0x66, 0x61, 0x66, 0x66, 0xc0,

    /* U+007E "~" */
    0xe6, 0x70,

    /* U+00A0 " " */
    0x0,

    /* U+00A1 "¡" */
    0xf3, 0xff, 0xc0,

    /* U+00A2 "¢" */
    0x10, 0x73, 0xf6, 0x8d, 0x1a, 0x3f, 0x1c, 0x10,

    /* U+00A3 "£" */
    0x38, 0xc9, 0x83, 0xf, 0x8c, 0x18, 0x33, 0xfc,

    /* U+00A4 "¤" */
    0xba, 0x8a, 0xc, 0x18, 0x30, 0x71, 0xdd,

    /* U+00A5 "¥" */
    0x44, 0xc8, 0xf1, 0xc7, 0xc3, 0x1f, 0xc, 0x18,

    /* U+00A6 "¦" */
    0xff, 0x3, 0xfc,

    /* U+00A7 "§" */
    0x39, 0xb6, 0xc, 0x5d, 0x17, 0xce, 0xd, 0xb3,
    0x80,

    /* U+00A8 "¨" */
    0xf0,

    /* U+00A9 "©" */
    0x3e, 0x31, 0xb6, 0x7c, 0xbe, 0x1f, 0x2f, 0x66,
    0xc6, 0x3e, 0x0,

    /* U+00AA "ª" */
    0xea, 0xfb, 0xf0,

    /* U+00AB "«" */
    0x6d, 0x6d, 0x34, 0x59, 0xb0,

    /* U+00AC "¬" */
    0xfc, 0x30, 0xc0,

    /* U+00AE "®" */
    0x38, 0x8a, 0xed, 0xda, 0xa8, 0x8e, 0x0,

    /* U+00AF "¯" */
    0xe0,

    /* U+00B0 "°" */
    0x69, 0x96,

    /* U+00B1 "±" */
    0x33, 0xf3, 0xc, 0x3, 0xf0,

    /* U+00B2 "²" */
    0xf9, 0x34, 0xf0,

    /* U+00B3 "³" */
    0x74, 0x88, 0x3f, 0x0,

    /* U+00B4 "´" */
    0x60,

    /* U+00B5 "µ" */
    0xcd, 0x9b, 0x36, 0x6c, 0xd9, 0xbf, 0xe0, 0xc1,
    0x80,

    /* U+00B6 "¶" */
    0x7f, 0xbe, 0xfb, 0xec, 0xb0, 0xc3, 0xc,

    /* U+00B7 "·" */
    0xf0,

    /* U+00B8 "¸" */
    0x9c,

    /* U+00B9 "¹" */
    0xd5, 0x40,

    /* U+00BA "º" */
    0x69, 0x99, 0x60,

    /* U+00BB "»" */
    0x58, 0x58, 0xd9, 0xb2, 0xcb, 0x0,

    /* U+00BC "¼" */
    0xc2, 0x22, 0x11, 0x9, 0x5, 0x30, 0x98, 0x94,
    0x8f, 0x41, 0x0,

    /* U+00BD "½" */
    0xc3, 0x10, 0x84, 0x41, 0x20, 0x4b, 0x84, 0xa2,
    0x8, 0x84, 0x43, 0xc0,

    /* U+00BE "¾" */
    0xe1, 0x8, 0xc2, 0x21, 0x10, 0x24, 0x3a, 0x61,
    0x98, 0x4a, 0x23, 0xc8, 0x20,

    /* U+00BF "¿" */
    0x30, 0xc0, 0xc, 0x31, 0x8c, 0x30, 0xcd, 0xe0,

    /* U+00C0 "À" */
    0x10, 0x8, 0x0, 0x1c, 0x1c, 0x34, 0x36, 0x36,
    0x66, 0x7f, 0x63, 0xc1,

    /* U+00C1 "Á" */
    0xc, 0x8, 0x0, 0x1c, 0x1c, 0x34, 0x36, 0x36,
    0x66, 0x7f, 0x63, 0xc1,

    /* U+00C2 "Â" */
    0x18, 0x14, 0x0, 0x1c, 0x1c, 0x34, 0x36, 0x36,
    0x66, 0x7f, 0x63, 0xc1,

    /* U+00C3 "Ã" */
    0x34, 0x2c, 0x0, 0x18, 0x38, 0x3c, 0x2c, 0x64,
    0x66, 0x7e, 0xc2, 0xc3,

    /* U+00C4 "Ä" */
    0x3c, 0x0, 0x0, 0x18, 0x18, 0x3c, 0x3c, 0x24,
    0x66, 0x7e, 0x42, 0xc3,

    /* U+00C5 "Å" */
    0x38, 0x28, 0x28, 0x18, 0x30, 0x38, 0x28, 0x6c,
    0x6c, 0x6c, 0xfe, 0xc6, 0x82,

    /* U+00C6 "Æ" */
    0xf, 0xf0, 0xb0, 0x1b, 0x1, 0xb0, 0x33, 0xf3,
    0x30, 0x3f, 0x6, 0x30, 0x43, 0xf0,

    /* U+00C7 "Ç" */
    0x3c, 0x62, 0xc3, 0xc0, 0xc0, 0xc0, 0xc3, 0x62,
    0x3c, 0x10, 0x8, 0x8,

    /* U+00C8 "È" */
    0x20, 0xc0, 0x3f, 0xc3, 0xc, 0x3f, 0xc3, 0xc,
    0x3f,

    /* U+00C9 "É" */
    0x18, 0x40, 0x3f, 0xc3, 0xc, 0x3f, 0xc3, 0xc,
    0x3f,

    /* U+00CA "Ê" */
    0x31, 0xa0, 0x3f, 0xc3, 0xc, 0x3f, 0xc3, 0xc,
    0x3f,

    /* U+00CB "Ë" */
    0x78, 0x0, 0x3f, 0xc3, 0xc, 0x3e, 0xc3, 0xc,
    0x3f,

    /* U+00CC "Ì" */
    0xc8, 0x36, 0xdb, 0x6d, 0xb0,

    /* U+00CD "Í" */
    0x68, 0x6d, 0xb6, 0xdb, 0x60,

    /* U+00CE "Î" */
    0x69, 0x6, 0x66, 0x66, 0x66, 0x66,

    /* U+00CF "Ï" */
    0xf0, 0x6, 0x66, 0x66, 0x66, 0x66,

    /* U+00D0 "Ð" */
    0x7e, 0x31, 0x98, 0x6c, 0x3f, 0x9b, 0xd, 0x86,
    0xc6, 0x7e, 0x0,

    /* U+00D1 "Ñ" */
    0x34, 0x28, 0x0, 0xe3, 0xe3, 0xf3, 0xd3, 0xdb,
    0xcb, 0xcf, 0xc7, 0xc7,

    /* U+00D2 "Ò" */
    0x10, 0x10, 0x0, 0x3c, 0x66, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0x66, 0x3c,

    /* U+00D3 "Ó" */
    0x8, 0x8, 0x0, 0x3c, 0x66, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0x66, 0x3c,

    /* U+00D4 "Ô" */
    0x18, 0x24, 0x0, 0x3c, 0x66, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0x66, 0x3c,

    /* U+00D5 "Õ" */
    0x14, 0x28, 0x0, 0x3c, 0x66, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0x66, 0x3c,

    /* U+00D6 "Ö" */
    0x3c, 0x0, 0x0, 0x3c, 0x66, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0x66, 0x3c,

    /* U+00D7 "×" */
    0x3, 0x37, 0x8c, 0x7b, 0x30,

    /* U+00D8 "Ø" */
    0x3e, 0x66, 0xcf, 0xcb, 0xdb, 0xd3, 0xf3, 0x66,
    0x7c,

    /* U+00D9 "Ù" */
    0x20, 0x20, 0x6, 0x3c, 0x78, 0xf1, 0xe3, 0xc7,
    0x8f, 0x19, 0xe0,

    /* U+00DA "Ú" */
    0x8, 0x20, 0x6, 0x3c, 0x78, 0xf1, 0xe3, 0xc7,
    0x8f, 0x19, 0xe0,

    /* U+00DB "Û" */
    0x10, 0x50, 0x6, 0x3c, 0x78, 0xf1, 0xe3, 0xc7,
    0x8f, 0x19, 0xe0,

    /* U+00DC "Ü" */
    0x6c, 0x0, 0x6, 0x3c, 0x78, 0xf1, 0xe3, 0xc7,
    0x8f, 0x19, 0xe0,

    /* U+00DD "Ý" */
    0xc, 0x18, 0x0, 0xc3, 0x66, 0x66, 0x3c, 0x3c,
    0x18, 0x18, 0x18, 0x18,

    /* U+00DE "Þ" */
    0xc3, 0xf, 0xb3, 0xcf, 0x3f, 0xb0, 0xc0,

    /* U+00DF "ß" */
    0x79, 0x9b, 0x36, 0x6d, 0x98, 0xf1, 0xe3, 0xdc,

    /* U+00E0 "à" */
    0x20, 0x80, 0x1e, 0xcc, 0x37, 0xf3, 0xcd, 0xf0,

    /* U+00E1 "á" */
    0x10, 0x40, 0x1e, 0xcc, 0x37, 0xf3, 0xcd, 0xf0,

    /* U+00E2 "â" */
    0x31, 0xe0, 0x1e, 0xcc, 0x37, 0xf3, 0xcd, 0xf0,

    /* U+00E3 "ã" */
    0x38, 0x7, 0xb3, 0xd, 0xfc, 0xf3, 0x7c,

    /* U+00E4 "ä" */
    0x78, 0x7, 0xb3, 0xd, 0xfc, 0xf3, 0x7c,

    /* U+00E5 "å" */
    0x31, 0x24, 0x8c, 0x7b, 0x30, 0xdf, 0xcf, 0x37,
    0xc0,

    /* U+00E6 "æ" */
    0x7f, 0xb3, 0x30, 0xcd, 0xff, 0xcc, 0x33, 0x37,
    0x78,

    /* U+00E7 "ç" */
    0x7b, 0x3c, 0x30, 0xc3, 0xb7, 0x84, 0x10, 0x40,

    /* U+00E8 "è" */
    0x20, 0x40, 0x1e, 0xcf, 0x1f, 0xf0, 0xcd, 0xe0,

    /* U+00E9 "é" */
    0x18, 0x40, 0x1e, 0xcf, 0x1f, 0xf0, 0xcd, 0xe0,

    /* U+00EA "ê" */
    0x30, 0xa0, 0x1e, 0xcf, 0x1f, 0xf0, 0xcd, 0xe0,

    /* U+00EB "ë" */
    0x78, 0x7, 0xb3, 0xcf, 0xfc, 0x33, 0x78,

    /* U+00EC "ì" */
    0xc8, 0x36, 0xdb, 0x6c,

    /* U+00ED "í" */
    0x78, 0x6d, 0xb6, 0xd8,

    /* U+00EE "î" */
    0x6f, 0x6, 0x66, 0x66, 0x66,

    /* U+00EF "ï" */
    0xf0, 0x66, 0x66, 0x66, 0x60,

    /* U+00F0 "ð" */
    0x28, 0xc1, 0x9e, 0xcf, 0x3c, 0xf3, 0xc9, 0xe0,

    /* U+00F1 "ñ" */
    0x78, 0xf, 0xb3, 0xcf, 0x3c, 0xf3, 0xcc,

    /* U+00F2 "ò" */
    0x20, 0x80, 0x1e, 0xcf, 0x3c, 0xf3, 0xcd, 0xe0,

    /* U+00F3 "ó" */
    0x10, 0x40, 0x1e, 0xcf, 0x3c, 0xf3, 0xcd, 0xe0,

    /* U+00F4 "ô" */
    0x30, 0xc0, 0x1e, 0xcf, 0x3c, 0xf3, 0xcd, 0xe0,

    /* U+00F5 "õ" */
    0x30, 0x7, 0xb3, 0xcf, 0x3c, 0xf3, 0x78,

    /* U+00F6 "ö" */
    0x78, 0x7, 0xb3, 0xcf, 0x3c, 0xf3, 0x78,

    /* U+00F7 "÷" */
    0x30, 0xc0, 0x3f, 0x0, 0xc3, 0x0,

    /* U+00F8 "ø" */
    0x7f, 0x3d, 0xfb, 0xef, 0x3f, 0x80,

    /* U+00F9 "ù" */
    0x20, 0x80, 0x33, 0xcf, 0x3c, 0xf3, 0xcd, 0xf0,

    /* U+00FA "ú" */
    0x10, 0x40, 0x33, 0xcf, 0x3c, 0xf3, 0xcd, 0xf0,

    /* U+00FB "û" */
    0x31, 0xe0, 0x33, 0xcf, 0x3c, 0xf3, 0xcd, 0xf0,

    /* U+00FC "ü" */
    0x78, 0xc, 0xf3, 0xcf, 0x3c, 0xf3, 0x7c,

    /* U+00FD "ý" */
    0x8, 0x23, 0x1a, 0x26, 0xcd, 0x8a, 0x14, 0x38,
    0x20, 0xc3, 0x0,

    /* U+00FE "þ" */
    0xc3, 0xf, 0xb3, 0xcf, 0x3c, 0xf3, 0xfb, 0xc,
    0x30,

    /* U+00FF "ÿ" */
    0x78, 0xc, 0xf3, 0x49, 0x27, 0x8c, 0x30, 0xc2,
    0x38,

    /* U+2013 "–" */
    0xfc,

    /* U+2014 "—" */
    0xff, 0xf0,

    /* U+2018 "‘" */
    0x7c,

    /* U+2019 "’" */
    0xf8,

    /* U+201A "‚" */
    0xe8,

    /* U+201B "‛" */
    0x49, 0x0,

    /* U+201C "“" */
    0x5e, 0xb4,

    /* U+201D "”" */
    0xd6, 0xa4,

    /* U+2022 "•" */
    0x6f, 0xf6,

    /* U+2026 "…" */
    0xcc, 0xf3, 0x30,

    /* U+203A "›" */
    0x42, 0x33, 0x24,

    /* U+2190 "←" */
    0x18, 0x18, 0x18, 0x1f, 0xf6, 0x1, 0x80, 0x60,

    /* U+2191 "↑" */
    0x10, 0x71, 0xf6, 0xb9, 0x22, 0x4, 0x8, 0x10,

    /* U+2192 "→" */
    0xc, 0x3, 0x0, 0x5f, 0xf0, 0x10, 0x30, 0x30,

    /* U+2193 "↓" */
    0x10, 0x20, 0x40, 0x89, 0x3a, 0xdf, 0x1c, 0x10,

    /* U+2713 "✓" */
    0x1, 0x1, 0x91, 0x9d, 0x87, 0x81, 0x80
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 48, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 62, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 100, .box_w = 4, .box_h = 3, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 6, .adv_w = 124, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 15, .adv_w = 125, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 25, .adv_w = 193, .box_w = 10, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 37, .adv_w = 127, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 46, .adv_w = 63, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 47, .adv_w = 72, .box_w = 3, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 52, .adv_w = 72, .box_w = 3, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 57, .adv_w = 104, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 61, .adv_w = 129, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 66, .adv_w = 61, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 67, .adv_w = 89, .box_w = 4, .box_h = 1, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 68, .adv_w = 61, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 69, .adv_w = 73, .box_w = 4, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 75, .adv_w = 127, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 83, .adv_w = 81, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 88, .adv_w = 120, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 95, .adv_w = 122, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 102, .adv_w = 128, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 110, .adv_w = 121, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 117, .adv_w = 123, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 125, .adv_w = 111, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 132, .adv_w = 123, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 139, .adv_w = 123, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 147, .adv_w = 61, .box_w = 2, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 149, .adv_w = 63, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 152, .adv_w = 129, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 158, .adv_w = 129, .box_w = 6, .box_h = 4, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 161, .adv_w = 129, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 167, .adv_w = 104, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 174, .adv_w = 192, .box_w = 11, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 191, .adv_w = 140, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 200, .adv_w = 127, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 208, .adv_w = 141, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 217, .adv_w = 139, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 225, .adv_w = 116, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 232, .adv_w = 113, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 239, .adv_w = 144, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 248, .adv_w = 143, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 256, .adv_w = 53, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 259, .adv_w = 111, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 266, .adv_w = 135, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 275, .adv_w = 109, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 282, .adv_w = 177, .box_w = 10, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 294, .adv_w = 146, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 303, .adv_w = 148, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 312, .adv_w = 124, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 320, .adv_w = 148, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 330, .adv_w = 125, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 338, .adv_w = 125, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 346, .adv_w = 127, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 354, .adv_w = 141, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 362, .adv_w = 140, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 371, .adv_w = 196, .box_w = 12, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 385, .adv_w = 138, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 394, .adv_w = 137, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 403, .adv_w = 125, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 410, .adv_w = 72, .box_w = 3, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 415, .adv_w = 73, .box_w = 4, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 421, .adv_w = 72, .box_w = 3, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 426, .adv_w = 92, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 5},
    {.bitmap_index = 429, .adv_w = 90, .box_w = 6, .box_h = 1, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 430, .adv_w = 67, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 431, .adv_w = 110, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 437, .adv_w = 120, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 444, .adv_w = 112, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 450, .adv_w = 120, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 457, .adv_w = 114, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 463, .adv_w = 75, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 468, .adv_w = 120, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 476, .adv_w = 118, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 483, .adv_w = 50, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 486, .adv_w = 50, .box_w = 3, .box_h = 12, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 491, .adv_w = 109, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 498, .adv_w = 50, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 501, .adv_w = 173, .box_w = 10, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 510, .adv_w = 117, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 516, .adv_w = 117, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 522, .adv_w = 120, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 530, .adv_w = 120, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 538, .adv_w = 76, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 542, .adv_w = 105, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 548, .adv_w = 68, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 553, .adv_w = 118, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 559, .adv_w = 113, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 566, .adv_w = 161, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 575, .adv_w = 109, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 582, .adv_w = 113, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 591, .adv_w = 109, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 596, .adv_w = 87, .box_w = 5, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 603, .adv_w = 69, .box_w = 2, .box_h = 15, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 607, .adv_w = 87, .box_w = 4, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 613, .adv_w = 129, .box_w = 6, .box_h = 2, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 615, .adv_w = 48, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 616, .adv_w = 62, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 619, .adv_w = 112, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 627, .adv_w = 121, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 635, .adv_w = 144, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 642, .adv_w = 108, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 650, .adv_w = 61, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 653, .adv_w = 111, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 662, .adv_w = 117, .box_w = 4, .box_h = 1, .ofs_x = 2, .ofs_y = 8},
    {.bitmap_index = 663, .adv_w = 176, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 674, .adv_w = 91, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 677, .adv_w = 122, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 682, .adv_w = 129, .box_w = 6, .box_h = 3, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 685, .adv_w = 128, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 692, .adv_w = 87, .box_w = 3, .box_h = 1, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 693, .adv_w = 88, .box_w = 4, .box_h = 4, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 695, .adv_w = 129, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 700, .adv_w = 87, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 703, .adv_w = 89, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 707, .adv_w = 67, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 708, .adv_w = 118, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 717, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 724, .adv_w = 61, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 725, .adv_w = 64, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 726, .adv_w = 62, .box_w = 2, .box_h = 5, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 728, .adv_w = 90, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 731, .adv_w = 122, .box_w = 7, .box_h = 6, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 737, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 748, .adv_w = 167, .box_w = 10, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 760, .adv_w = 174, .box_w = 10, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 773, .adv_w = 104, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 781, .adv_w = 140, .box_w = 8, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 793, .adv_w = 140, .box_w = 8, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 805, .adv_w = 140, .box_w = 8, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 817, .adv_w = 140, .box_w = 8, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 829, .adv_w = 140, .box_w = 8, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 841, .adv_w = 140, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 854, .adv_w = 195, .box_w = 12, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 868, .adv_w = 141, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 880, .adv_w = 116, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 889, .adv_w = 116, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 898, .adv_w = 116, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 907, .adv_w = 116, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 916, .adv_w = 53, .box_w = 3, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 921, .adv_w = 53, .box_w = 3, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 926, .adv_w = 53, .box_w = 4, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 932, .adv_w = 53, .box_w = 4, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 938, .adv_w = 144, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 949, .adv_w = 146, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 961, .adv_w = 148, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 973, .adv_w = 148, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 985, .adv_w = 148, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 997, .adv_w = 148, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1009, .adv_w = 148, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1021, .adv_w = 129, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 1026, .adv_w = 148, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1035, .adv_w = 141, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1046, .adv_w = 141, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1057, .adv_w = 141, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1068, .adv_w = 141, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1079, .adv_w = 137, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1091, .adv_w = 126, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1098, .adv_w = 124, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1106, .adv_w = 110, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1114, .adv_w = 110, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1122, .adv_w = 110, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1130, .adv_w = 110, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1137, .adv_w = 110, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1144, .adv_w = 110, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1153, .adv_w = 175, .box_w = 10, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1162, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1170, .adv_w = 114, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1178, .adv_w = 114, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1186, .adv_w = 114, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1194, .adv_w = 114, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1201, .adv_w = 50, .box_w = 3, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1205, .adv_w = 50, .box_w = 3, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1209, .adv_w = 50, .box_w = 4, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1214, .adv_w = 50, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1219, .adv_w = 114, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1227, .adv_w = 117, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1234, .adv_w = 117, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1242, .adv_w = 117, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1250, .adv_w = 117, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1258, .adv_w = 117, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1265, .adv_w = 117, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1272, .adv_w = 129, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1278, .adv_w = 117, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1284, .adv_w = 118, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1292, .adv_w = 118, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1300, .adv_w = 118, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1308, .adv_w = 118, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1315, .adv_w = 113, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1326, .adv_w = 120, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1335, .adv_w = 113, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1344, .adv_w = 96, .box_w = 6, .box_h = 1, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 1345, .adv_w = 192, .box_w = 12, .box_h = 1, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 1347, .adv_w = 56, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 1348, .adv_w = 56, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 1349, .adv_w = 52, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1350, .adv_w = 56, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 1352, .adv_w = 97, .box_w = 5, .box_h = 3, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 1354, .adv_w = 96, .box_w = 5, .box_h = 3, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 1356, .adv_w = 97, .box_w = 4, .box_h = 4, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 1358, .adv_w = 184, .box_w = 10, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1361, .adv_w = 80, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1364, .adv_w = 183, .box_w = 9, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1372, .adv_w = 169, .box_w = 7, .box_h = 9, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 1380, .adv_w = 183, .box_w = 9, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1388, .adv_w = 169, .box_w = 7, .box_h = 9, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 1396, .adv_w = 170, .box_w = 9, .box_h = 6, .ofs_x = 1, .ofs_y = 1}
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
const lv_font_t phos_inter_16_cs = {
#else
lv_font_t phos_inter_16_cs = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 16,          /*The maximum line height required by the font*/
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



#endif /*#if PHOS_INTER_16_CS*/

