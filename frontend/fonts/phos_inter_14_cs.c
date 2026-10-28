/*******************************************************************************
 * Size: 10 px
 * Bpp: 1
 * Opts: --no-compress --no-prefilter --bpp 1 --size 10 --font /usr/share/fonts/opentype/inter/Inter-SemiBold.otf -r 0x20-0x7E,0xA0-0xFF,0x2013-0x2014,0x2018-0x201D,0x2022,0x2026,0x203A,0x2190-0x2193,0x2713 --format lvgl --lv-include lvgl.h -o fonts/phos_inter_14_cs.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef PHOS_INTER_14_CS
#define PHOS_INTER_14_CS 1
#endif

#if PHOS_INTER_14_CS

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xf9,

    /* U+0022 "\"" */
    0xb6, 0x80,

    /* U+0023 "#" */
    0x24, 0xa7, 0xca, 0x4b, 0xf4, 0x94,

    /* U+0024 "$" */
    0x23, 0xab, 0x4e, 0x3c, 0xb5, 0x71, 0x0,

    /* U+0025 "%" */
    0xe2, 0xa4, 0xa8, 0xe8, 0x17, 0x25, 0x25, 0x47,

    /* U+0026 "&" */
    0x71, 0x45, 0x18, 0xea, 0x68, 0x9f,

    /* U+0027 "'" */
    0xe0,

    /* U+0028 "(" */
    0x7a, 0xaa, 0xd0,

    /* U+0029 ")" */
    0xb5, 0x55, 0xe0,

    /* U+002A "*" */
    0x23, 0x9c, 0x40,

    /* U+002B "+" */
    0x21, 0x3e, 0x42, 0x0,

    /* U+002C "," */
    0xe0,

    /* U+002D "-" */
    0xe0,

    /* U+002E "." */
    0x80,

    /* U+002F "/" */
    0x12, 0x22, 0x24, 0x44, 0x48,

    /* U+0030 "0" */
    0x76, 0xe3, 0x18, 0xc7, 0x6e,

    /* U+0031 "1" */
    0x7e, 0x92, 0x49,

    /* U+0032 "2" */
    0x74, 0x42, 0x31, 0x11, 0x1f,

    /* U+0033 "3" */
    0x72, 0x42, 0x60, 0x85, 0x2e,

    /* U+0034 "4" */
    0x18, 0xe2, 0x9a, 0xcb, 0xf0, 0x82,

    /* U+0035 "5" */
    0xfe, 0x31, 0x64, 0x87, 0x2e,

    /* U+0036 "6" */
    0x76, 0x61, 0xe8, 0xc6, 0x2e,

    /* U+0037 "7" */
    0xf8, 0xc4, 0x22, 0x11, 0x88,

    /* U+0038 "8" */
    0x74, 0x62, 0xe8, 0xc6, 0x2e,

    /* U+0039 "9" */
    0x74, 0x63, 0x17, 0x86, 0x4e,

    /* U+003A ":" */
    0x88,

    /* U+003B ";" */
    0x8e,

    /* U+003C "<" */
    0x9, 0xb1, 0xc3, 0x84,

    /* U+003D "=" */
    0xf8, 0x1, 0xf0,

    /* U+003E ">" */
    0x83, 0x6, 0x7e, 0x40,

    /* U+003F "?" */
    0x72, 0x42, 0x33, 0x10, 0x4,

    /* U+0040 "@" */
    0x3e, 0x31, 0xb7, 0xf4, 0x5a, 0x2d, 0x16, 0x8b,
    0xbe, 0x60, 0x1f, 0x0,

    /* U+0041 "A" */
    0x18, 0x70, 0xa1, 0x66, 0xcf, 0x91, 0xe3,

    /* U+0042 "B" */
    0xf4, 0x63, 0xe8, 0xc6, 0x3e,

    /* U+0043 "C" */
    0x79, 0x38, 0x60, 0x82, 0x14, 0xce,

    /* U+0044 "D" */
    0xf2, 0x28, 0x61, 0x86, 0x18, 0xbc,

    /* U+0045 "E" */
    0xf8, 0x88, 0xf8, 0x8f,

    /* U+0046 "F" */
    0xf8, 0x88, 0xf8, 0x88,

    /* U+0047 "G" */
    0x79, 0x38, 0x60, 0x9e, 0x14, 0xde,

    /* U+0048 "H" */
    0x86, 0x18, 0x7f, 0x86, 0x18, 0x61,

    /* U+0049 "I" */
    0xff,

    /* U+004A "J" */
    0x8, 0x42, 0x10, 0xc6, 0x2e,

    /* U+004B "K" */
    0x8a, 0x6b, 0x38, 0xf2, 0x49, 0xa2,

    /* U+004C "L" */
    0x88, 0x88, 0x88, 0x8f,

    /* U+004D "M" */
    0xc3, 0xc3, 0xe7, 0xa5, 0xb5, 0xbd, 0x99, 0x99,

    /* U+004E "N" */
    0xc7, 0x1e, 0x69, 0x96, 0x78, 0xe3,

    /* U+004F "O" */
    0x38, 0x8a, 0xc, 0x18, 0x30, 0x51, 0x1c,

    /* U+0050 "P" */
    0xf4, 0x63, 0x1f, 0x42, 0x10,

    /* U+0051 "Q" */
    0x38, 0x8a, 0xc, 0x18, 0x33, 0x53, 0x9e, 0x2,

    /* U+0052 "R" */
    0xf4, 0x63, 0x1f, 0x4a, 0x71,

    /* U+0053 "S" */
    0x74, 0x61, 0xc3, 0x86, 0x2e,

    /* U+0054 "T" */
    0xfc, 0x82, 0x8, 0x20, 0x82, 0x8,

    /* U+0055 "U" */
    0x86, 0x18, 0x61, 0x86, 0x1c, 0xde,

    /* U+0056 "V" */
    0xc6, 0x8d, 0x13, 0x22, 0xc5, 0xe, 0xc,

    /* U+0057 "W" */
    0xcc, 0xd3, 0x34, 0xc9, 0x3a, 0x7a, 0x8c, 0xe3,
    0x30, 0xcc,

    /* U+0058 "X" */
    0x44, 0xd8, 0xa0, 0xc3, 0x85, 0x1b, 0x23,

    /* U+0059 "Y" */
    0x44, 0xd8, 0xa1, 0x41, 0x2, 0x4, 0x8,

    /* U+005A "Z" */
    0xf8, 0xc4, 0x42, 0x23, 0x1f,

    /* U+005B "[" */
    0xea, 0xaa, 0xb0,

    /* U+005C "\\" */
    0x8c, 0x44, 0x46, 0x22, 0x23,

    /* U+005D "]" */
    0xd5, 0x55, 0x70,

    /* U+005E "^" */
    0x26, 0x55,

    /* U+005F "_" */
    0xf8,

    /* U+0060 "`" */
    0x80,

    /* U+0061 "a" */
    0x74, 0xcf, 0x98, 0xbc,

    /* U+0062 "b" */
    0x84, 0x3d, 0x18, 0xc6, 0x3e,

    /* U+0063 "c" */
    0x74, 0xe1, 0x9, 0xb8,

    /* U+0064 "d" */
    0x8, 0x5f, 0x18, 0xc6, 0x2f,

    /* U+0065 "e" */
    0x74, 0xff, 0x9, 0xb8,

    /* U+0066 "f" */
    0x6b, 0xa4, 0x92,

    /* U+0067 "g" */
    0x7c, 0x63, 0x18, 0xbc, 0x2e,

    /* U+0068 "h" */
    0x84, 0x3d, 0x18, 0xc6, 0x31,

    /* U+0069 "i" */
    0xbf,

    /* U+006A "j" */
    0x45, 0x55, 0x70,

    /* U+006B "k" */
    0x84, 0x25, 0x4e, 0x52, 0xd2,

    /* U+006C "l" */
    0xff,

    /* U+006D "m" */
    0xef, 0x26, 0x4c, 0x99, 0x32, 0x40,

    /* U+006E "n" */
    0xf4, 0x63, 0x18, 0xc4,

    /* U+006F "o" */
    0x74, 0x63, 0x18, 0xb8,

    /* U+0070 "p" */
    0xf4, 0x63, 0x18, 0xfa, 0x10,

    /* U+0071 "q" */
    0x7c, 0x63, 0x18, 0xbc, 0x21,

    /* U+0072 "r" */
    0xf2, 0x49, 0x0,

    /* U+0073 "s" */
    0x69, 0xe3, 0x9e,

    /* U+0074 "t" */
    0x4b, 0xa4, 0x93,

    /* U+0075 "u" */
    0x8c, 0x63, 0x18, 0xbc,

    /* U+0076 "v" */
    0xcd, 0x24, 0x9c, 0x30, 0xc0,

    /* U+0077 "w" */
    0xd9, 0x5b, 0x5b, 0x56, 0x76, 0x26,

    /* U+0078 "x" */
    0x4b, 0x8c, 0x65, 0xa4,

    /* U+0079 "y" */
    0xcd, 0x24, 0x9c, 0x30, 0xc2, 0x18,

    /* U+007A "z" */
    0xf3, 0x24, 0xcf,

    /* U+007B "{" */
    0x69, 0x28, 0x92, 0x44,

    /* U+007C "|" */
    0xff, 0xf8,

    /* U+007D "}" */
    0xc4, 0x44, 0x34, 0x44, 0x48,

    /* U+007E "~" */
    0xed, 0xc0,

    /* U+00A0 " " */
    0x0,

    /* U+00A1 "¡" */
    0xbe,

    /* U+00A2 "¢" */
    0x23, 0xab, 0x4a, 0x55, 0xc4,

    /* U+00A3 "£" */
    0x39, 0x24, 0x10, 0xf1, 0x4, 0xfe,

    /* U+00A4 "¤" */
    0xb5, 0x28, 0x61, 0x85, 0x2b, 0x40,

    /* U+00A5 "¥" */
    0xda, 0x94, 0xef, 0xfc, 0x84,

    /* U+00A6 "¦" */
    0xe7,

    /* U+00A7 "§" */
    0x31, 0x26, 0x1e, 0x49, 0xa1, 0x92, 0x70,

    /* U+00A8 "¨" */
    0xa0,

    /* U+00A9 "©" */
    0x38, 0x8a, 0x6d, 0x1a, 0xb3, 0x51, 0x1c,

    /* U+00AA "ª" */
    0xf7, 0xde,

    /* U+00AB "«" */
    0x2d, 0xed, 0x1e, 0x2c,

    /* U+00AC "¬" */
    0xf1, 0x10,

    /* U+00AE "®" */
    0x7b, 0xfb, 0x6d, 0xfd, 0xe0,

    /* U+00AF "¯" */
    0xe0,

    /* U+00B0 "°" */
    0x56, 0xa0,

    /* U+00B1 "±" */
    0x21, 0x3e, 0x40, 0x7c,

    /* U+00B2 "²" */
    0xea, 0x4f,

    /* U+00B3 "³" */
    0xe4, 0x3e,

    /* U+00B4 "´" */
    0x80,

    /* U+00B5 "µ" */
    0x8a, 0x28, 0xa2, 0x8b, 0xf8, 0x20,

    /* U+00B6 "¶" */
    0x7f, 0x7b, 0xd6, 0x84, 0x21,

    /* U+00B7 "·" */
    0x80,

    /* U+00B8 "¸" */
    0xf0,

    /* U+00B9 "¹" */
    0xd5,

    /* U+00BA "º" */
    0x69, 0x99, 0x60,

    /* U+00BB "»" */
    0x50, 0xa3, 0xca, 0x50,

    /* U+00BC "¼" */
    0xc4, 0x91, 0x22, 0x91, 0x65, 0x53, 0xa1,

    /* U+00BD "½" */
    0xc4, 0x44, 0x48, 0x57, 0x15, 0x21, 0x22, 0x47,

    /* U+00BE "¾" */
    0xe2, 0x44, 0x24, 0xeb, 0x13, 0x15, 0x27, 0x41,

    /* U+00BF "¿" */
    0x20, 0x8, 0xcc, 0x42, 0x4e,

    /* U+00C0 "À" */
    0x10, 0x0, 0x61, 0xc2, 0x85, 0x9b, 0x3e, 0x47,
    0x8c,

    /* U+00C1 "Á" */
    0x18, 0x0, 0x61, 0xc2, 0x85, 0x9b, 0x3e, 0x47,
    0x8c,

    /* U+00C2 "Â" */
    0x10, 0x70, 0xa0, 0xc3, 0x85, 0xb, 0x36, 0x7c,
    0x8f, 0x18,

    /* U+00C3 "Ã" */
    0x38, 0x0, 0x61, 0x42, 0x85, 0x9b, 0x3e, 0x47,
    0x8c,

    /* U+00C4 "Ä" */
    0x28, 0x0, 0x41, 0x42, 0x85, 0x1b, 0x3e, 0x45,
    0x8c,

    /* U+00C5 "Å" */
    0x30, 0x50, 0xc1, 0x83, 0x85, 0x1a, 0x24, 0x7d,
    0x8b, 0x10,

    /* U+00C6 "Æ" */
    0x1f, 0x8a, 0x5, 0x6, 0x82, 0x7b, 0xe1, 0x10,
    0x8f,

    /* U+00C7 "Ç" */
    0x39, 0x38, 0x60, 0x82, 0x14, 0xce, 0x10, 0x40,

    /* U+00C8 "È" */
    0x40, 0xf8, 0x88, 0xf8, 0x8f,

    /* U+00C9 "É" */
    0x20, 0xf8, 0x88, 0xf8, 0x8f,

    /* U+00CA "Ê" */
    0x26, 0x9f, 0x88, 0x8f, 0x88, 0xf0,

    /* U+00CB "Ë" */
    0x50, 0x3f, 0x8, 0x43, 0xd0, 0x87, 0xc0,

    /* U+00CC "Ì" */
    0x45, 0x55, 0x50,

    /* U+00CD "Í" */
    0x8a, 0xaa, 0xa0,

    /* U+00CE "Î" */
    0x56, 0xa4, 0x92, 0x49, 0x0,

    /* U+00CF "Ï" */
    0xa1, 0x24, 0x92, 0x48,

    /* U+00D0 "Ð" */
    0x78, 0x89, 0xa, 0x1f, 0x28, 0x51, 0x3c,

    /* U+00D1 "Ñ" */
    0x78, 0xc, 0x71, 0xe6, 0x99, 0x67, 0x8e, 0x30,

    /* U+00D2 "Ò" */
    0x30, 0x0, 0xe2, 0x28, 0x30, 0x60, 0xc1, 0x44,
    0x70,

    /* U+00D3 "Ó" */
    0x18, 0x0, 0xe2, 0x28, 0x30, 0x60, 0xc1, 0x44,
    0x70,

    /* U+00D4 "Ô" */
    0x10, 0x50, 0x1, 0xc4, 0x50, 0x60, 0xc1, 0x82,
    0x88, 0xe0,

    /* U+00D5 "Õ" */
    0x38, 0x0, 0xe2, 0x28, 0x30, 0x60, 0xc1, 0x44,
    0x70,

    /* U+00D6 "Ö" */
    0x28, 0x0, 0xe2, 0x28, 0x30, 0x60, 0xc1, 0x44,
    0x70,

    /* U+00D7 "×" */
    0x86, 0x9c, 0xed, 0x80,

    /* U+00D8 "Ø" */
    0x3e, 0x8e, 0x2c, 0xdb, 0x34, 0x71, 0x7c,

    /* U+00D9 "Ù" */
    0x20, 0x8, 0x61, 0x86, 0x18, 0x61, 0xcd, 0xe0,

    /* U+00DA "Ú" */
    0x10, 0x8, 0x61, 0x86, 0x18, 0x61, 0xcd, 0xe0,

    /* U+00DB "Û" */
    0x31, 0xe0, 0x21, 0x86, 0x18, 0x61, 0x87, 0x37,
    0x80,

    /* U+00DC "Ü" */
    0x48, 0x8, 0x61, 0x86, 0x18, 0x61, 0xcd, 0xe0,

    /* U+00DD "Ý" */
    0x10, 0x1, 0x13, 0x62, 0x85, 0x4, 0x8, 0x10,
    0x20,

    /* U+00DE "Þ" */
    0x84, 0x3d, 0x18, 0xfa, 0x10,

    /* U+00DF "ß" */
    0x74, 0x63, 0x68, 0xc6, 0x36,

    /* U+00E0 "à" */
    0x60, 0x1d, 0x33, 0xe6, 0x2f,

    /* U+00E1 "á" */
    0x30, 0x1d, 0x33, 0xe6, 0x2f,

    /* U+00E2 "â" */
    0x22, 0x80, 0xe9, 0x9f, 0x31, 0x78,

    /* U+00E3 "ã" */
    0x70, 0x1d, 0x33, 0xe6, 0x2f,

    /* U+00E4 "ä" */
    0x50, 0x1d, 0x33, 0xe6, 0x2f,

    /* U+00E5 "å" */
    0x72, 0x9c, 0xe9, 0x9f, 0x31, 0x78,

    /* U+00E6 "æ" */
    0x6e, 0x11, 0x7f, 0x90, 0x91, 0xee,

    /* U+00E7 "ç" */
    0x74, 0x61, 0x8, 0xb0, 0xc6,

    /* U+00E8 "è" */
    0x40, 0x1d, 0x3f, 0xc2, 0x6e,

    /* U+00E9 "é" */
    0x20, 0x1d, 0x3f, 0xc2, 0x6e,

    /* U+00EA "ê" */
    0x22, 0x80, 0xe9, 0xfe, 0x13, 0x70,

    /* U+00EB "ë" */
    0x50, 0x1d, 0x3f, 0xc2, 0x6e,

    /* U+00EC "ì" */
    0x45, 0x55,

    /* U+00ED "í" */
    0x8a, 0xaa,

    /* U+00EE "î" */
    0x54, 0x24, 0x92, 0x40,

    /* U+00EF "ï" */
    0xa1, 0x24, 0x92,

    /* U+00F0 "ð" */
    0x41, 0x94, 0xd9, 0xc6, 0x31, 0x70,

    /* U+00F1 "ñ" */
    0x70, 0x3d, 0x18, 0xc6, 0x31,

    /* U+00F2 "ò" */
    0x60, 0x1d, 0x18, 0xc6, 0x2e,

    /* U+00F3 "ó" */
    0x30, 0x1d, 0x18, 0xc6, 0x2e,

    /* U+00F4 "ô" */
    0x22, 0x80, 0xe8, 0xc6, 0x31, 0x70,

    /* U+00F5 "õ" */
    0x70, 0x1d, 0x18, 0xc6, 0x2e,

    /* U+00F6 "ö" */
    0x50, 0x1d, 0x18, 0xc6, 0x2e,

    /* U+00F7 "÷" */
    0x20, 0x1, 0xf0, 0x10,

    /* U+00F8 "ø" */
    0x7c, 0xeb, 0xdc, 0xf8,

    /* U+00F9 "ù" */
    0x60, 0x23, 0x18, 0xc6, 0x2f,

    /* U+00FA "ú" */
    0x30, 0x23, 0x18, 0xc6, 0x2f,

    /* U+00FB "û" */
    0x22, 0x81, 0x18, 0xc6, 0x31, 0x78,

    /* U+00FC "ü" */
    0x50, 0x23, 0x18, 0xc6, 0x2f,

    /* U+00FD "ý" */
    0x10, 0xc, 0xd2, 0x49, 0xc3, 0xc, 0x21, 0x80,

    /* U+00FE "þ" */
    0x84, 0x3d, 0x18, 0xc6, 0x3e, 0x84, 0x0,

    /* U+00FF "ÿ" */
    0x50, 0x23, 0xb5, 0x29, 0xc4, 0x22, 0x0,

    /* U+2013 "–" */
    0xf8,

    /* U+2014 "—" */
    0xff, 0xc0,

    /* U+2018 "‘" */
    0xa8,

    /* U+2019 "’" */
    0xa8,

    /* U+201A "‚" */
    0xe0,

    /* U+201B "‛" */
    0x54,

    /* U+201C "“" */
    0x9a, 0xa0,

    /* U+201D "”" */
    0xaa, 0xa0,

    /* U+2022 "•" */
    0xff, 0x80,

    /* U+2026 "…" */
    0x92,

    /* U+203A "›" */
    0x42, 0x32, 0x40,

    /* U+2190 "←" */
    0x10, 0x41, 0x7, 0xf4, 0x4, 0x4, 0x0,

    /* U+2191 "↑" */
    0x10, 0x71, 0x50, 0x81, 0x2, 0x4, 0x0,

    /* U+2192 "→" */
    0x8, 0xc, 0x6, 0xff, 0x6, 0xc, 0x8,

    /* U+2193 "↓" */
    0x10, 0x20, 0x44, 0x95, 0x47, 0x4, 0x0,

    /* U+2713 "✓" */
    0x2, 0xe, 0x32, 0xc2, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 40, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 51, .box_w = 1, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 2, .adv_w = 84, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 4, .adv_w = 103, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 10, .adv_w = 104, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 17, .adv_w = 161, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 25, .adv_w = 106, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 31, .adv_w = 52, .box_w = 1, .box_h = 3, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 32, .adv_w = 60, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 35, .adv_w = 60, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 38, .adv_w = 86, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 41, .adv_w = 108, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 45, .adv_w = 51, .box_w = 1, .box_h = 3, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 46, .adv_w = 74, .box_w = 3, .box_h = 1, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 47, .adv_w = 51, .box_w = 1, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 48, .adv_w = 61, .box_w = 4, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 53, .adv_w = 106, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 58, .adv_w = 68, .box_w = 3, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 61, .adv_w = 100, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 66, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 71, .adv_w = 107, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 77, .adv_w = 101, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 82, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 87, .adv_w = 92, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 92, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 97, .adv_w = 102, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 102, .adv_w = 51, .box_w = 1, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 103, .adv_w = 53, .box_w = 1, .box_h = 7, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 104, .adv_w = 108, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 108, .adv_w = 108, .box_w = 5, .box_h = 4, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 111, .adv_w = 108, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 115, .adv_w = 87, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 120, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 132, .adv_w = 116, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 139, .adv_w = 105, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 144, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 150, .adv_w = 116, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 156, .adv_w = 97, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 160, .adv_w = 94, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 164, .adv_w = 120, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 170, .adv_w = 119, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 176, .adv_w = 44, .box_w = 1, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 177, .adv_w = 93, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 182, .adv_w = 113, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 188, .adv_w = 90, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 192, .adv_w = 148, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 200, .adv_w = 121, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 206, .adv_w = 123, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 213, .adv_w = 103, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 218, .adv_w = 124, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 226, .adv_w = 104, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 231, .adv_w = 104, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 236, .adv_w = 106, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 242, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 248, .adv_w = 116, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 255, .adv_w = 163, .box_w = 10, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 265, .adv_w = 115, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 272, .adv_w = 114, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 279, .adv_w = 104, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 284, .adv_w = 60, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 287, .adv_w = 61, .box_w = 4, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 292, .adv_w = 60, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 295, .adv_w = 77, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 297, .adv_w = 75, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 298, .adv_w = 56, .box_w = 2, .box_h = 1, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 299, .adv_w = 92, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 303, .adv_w = 100, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 308, .adv_w = 93, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 312, .adv_w = 100, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 317, .adv_w = 95, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 321, .adv_w = 62, .box_w = 3, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 324, .adv_w = 100, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 329, .adv_w = 98, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 334, .adv_w = 42, .box_w = 1, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 335, .adv_w = 42, .box_w = 2, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 338, .adv_w = 91, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 343, .adv_w = 42, .box_w = 1, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 344, .adv_w = 144, .box_w = 7, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 350, .adv_w = 98, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 354, .adv_w = 97, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 358, .adv_w = 100, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 363, .adv_w = 100, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 368, .adv_w = 64, .box_w = 3, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 371, .adv_w = 88, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 374, .adv_w = 56, .box_w = 3, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 377, .adv_w = 98, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 381, .adv_w = 94, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 386, .adv_w = 134, .box_w = 8, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 392, .adv_w = 91, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 396, .adv_w = 94, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 402, .adv_w = 91, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 405, .adv_w = 73, .box_w = 3, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 409, .adv_w = 57, .box_w = 1, .box_h = 13, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 411, .adv_w = 73, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 416, .adv_w = 108, .box_w = 5, .box_h = 2, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 418, .adv_w = 40, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 419, .adv_w = 51, .box_w = 1, .box_h = 7, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 420, .adv_w = 93, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 425, .adv_w = 101, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 431, .adv_w = 120, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 437, .adv_w = 90, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 442, .adv_w = 51, .box_w = 1, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 443, .adv_w = 92, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 450, .adv_w = 98, .box_w = 3, .box_h = 1, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 451, .adv_w = 146, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 458, .adv_w = 76, .box_w = 3, .box_h = 5, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 460, .adv_w = 101, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 464, .adv_w = 108, .box_w = 4, .box_h = 3, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 466, .adv_w = 107, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 471, .adv_w = 72, .box_w = 3, .box_h = 1, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 472, .adv_w = 73, .box_w = 3, .box_h = 4, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 474, .adv_w = 108, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 478, .adv_w = 73, .box_w = 4, .box_h = 4, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 480, .adv_w = 74, .box_w = 4, .box_h = 4, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 482, .adv_w = 56, .box_w = 2, .box_h = 1, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 483, .adv_w = 98, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 489, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 494, .adv_w = 51, .box_w = 1, .box_h = 1, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 495, .adv_w = 53, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 496, .adv_w = 52, .box_w = 2, .box_h = 4, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 497, .adv_w = 75, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 500, .adv_w = 101, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 504, .adv_w = 133, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 511, .adv_w = 139, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 519, .adv_w = 145, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 527, .adv_w = 87, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 532, .adv_w = 116, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 541, .adv_w = 116, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 550, .adv_w = 116, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 560, .adv_w = 116, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 569, .adv_w = 116, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 578, .adv_w = 116, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 588, .adv_w = 162, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 597, .adv_w = 118, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 605, .adv_w = 97, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 610, .adv_w = 97, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 615, .adv_w = 97, .box_w = 4, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 621, .adv_w = 97, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 628, .adv_w = 44, .box_w = 2, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 631, .adv_w = 44, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 634, .adv_w = 44, .box_w = 3, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 639, .adv_w = 44, .box_w = 3, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 643, .adv_w = 120, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 650, .adv_w = 121, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 658, .adv_w = 123, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 667, .adv_w = 123, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 676, .adv_w = 123, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 686, .adv_w = 123, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 695, .adv_w = 123, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 704, .adv_w = 108, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 708, .adv_w = 123, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 715, .adv_w = 118, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 723, .adv_w = 118, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 731, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 740, .adv_w = 118, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 748, .adv_w = 114, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 757, .adv_w = 105, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 762, .adv_w = 103, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 767, .adv_w = 92, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 772, .adv_w = 92, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 777, .adv_w = 92, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 783, .adv_w = 92, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 788, .adv_w = 92, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 793, .adv_w = 92, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 799, .adv_w = 146, .box_w = 8, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 805, .adv_w = 93, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 810, .adv_w = 95, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 815, .adv_w = 95, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 820, .adv_w = 95, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 826, .adv_w = 95, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 831, .adv_w = 42, .box_w = 2, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 833, .adv_w = 42, .box_w = 2, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 835, .adv_w = 42, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 839, .adv_w = 42, .box_w = 3, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 842, .adv_w = 95, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 848, .adv_w = 98, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 853, .adv_w = 97, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 858, .adv_w = 97, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 863, .adv_w = 97, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 869, .adv_w = 97, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 874, .adv_w = 97, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 879, .adv_w = 108, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 883, .adv_w = 97, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 887, .adv_w = 98, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 892, .adv_w = 98, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 897, .adv_w = 98, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 903, .adv_w = 98, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 908, .adv_w = 94, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 916, .adv_w = 100, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 923, .adv_w = 94, .box_w = 5, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 930, .adv_w = 80, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 931, .adv_w = 160, .box_w = 10, .box_h = 1, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 933, .adv_w = 47, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 934, .adv_w = 47, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 935, .adv_w = 44, .box_w = 1, .box_h = 3, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 936, .adv_w = 47, .box_w = 2, .box_h = 3, .ofs_x = 0, .ofs_y = 5},
    {.bitmap_index = 937, .adv_w = 81, .box_w = 4, .box_h = 3, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 939, .adv_w = 80, .box_w = 4, .box_h = 3, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 941, .adv_w = 81, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 943, .adv_w = 153, .box_w = 7, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 944, .adv_w = 66, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 947, .adv_w = 153, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 954, .adv_w = 141, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 961, .adv_w = 153, .box_w = 8, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 968, .adv_w = 141, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 975, .adv_w = 141, .box_w = 7, .box_h = 5, .ofs_x = 1, .ofs_y = 1}
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
const lv_font_t phos_inter_14_cs = {
#else
lv_font_t phos_inter_14_cs = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 14,          /*The maximum line height required by the font*/
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



#endif /*#if PHOS_INTER_14_CS*/

