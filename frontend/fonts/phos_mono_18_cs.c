/*******************************************************************************
 * Size: 10 px
 * Bpp: 1
 * Opts: --no-compress --no-prefilter --bpp 1 --size 10 --font /usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf -r 0x20-0x7E,0xA0-0xFF,0x2014,0x2022,0x2026,0x2190-0x2195,0x2261 --format lvgl --lv-include lvgl.h -o fonts/phos_mono_18_cs.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef PHOS_MONO_18_CS
#define PHOS_MONO_18_CS 1
#endif

#if PHOS_MONO_18_CS

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
    0x28, 0xa7, 0xca, 0x53, 0xe5, 0x14,

    /* U+0024 "$" */
    0x23, 0xa9, 0x47, 0x14, 0xbe, 0x21, 0x0,

    /* U+0025 "%" */
    0x42, 0x8a, 0x32, 0x31, 0x62, 0x4e,

    /* U+0026 "&" */
    0x71, 0x4, 0x18, 0x96, 0x5c, 0x9d,

    /* U+0027 "'" */
    0xe0,

    /* U+0028 "(" */
    0x5a, 0xaa, 0x40,

    /* U+0029 ")" */
    0xa5, 0x56, 0xa0,

    /* U+002A "*" */
    0x25, 0x5d, 0xf2, 0x0,

    /* U+002B "+" */
    0x21, 0x3e, 0x42, 0x0,

    /* U+002C "," */
    0xe0,

    /* U+002D "-" */
    0xc0,

    /* U+002E "." */
    0x80,

    /* U+002F "/" */
    0x8, 0x44, 0x22, 0x10, 0x88, 0x40,

    /* U+0030 "0" */
    0x76, 0xe3, 0x58, 0xc7, 0x6e,

    /* U+0031 "1" */
    0xe1, 0x8, 0x42, 0x10, 0x9f,

    /* U+0032 "2" */
    0x70, 0x42, 0x11, 0x11, 0x1f,

    /* U+0033 "3" */
    0x70, 0x42, 0x60, 0x86, 0x3e,

    /* U+0034 "4" */
    0x11, 0x94, 0xa9, 0x7c, 0x42,

    /* U+0035 "5" */
    0x72, 0x10, 0xe0, 0x84, 0x3e,

    /* U+0036 "6" */
    0x76, 0x21, 0xe8, 0xc6, 0x2e,

    /* U+0037 "7" */
    0xf8, 0x44, 0x21, 0x10, 0x84,

    /* U+0038 "8" */
    0x69, 0x96, 0x99, 0x96,

    /* U+0039 "9" */
    0x74, 0x63, 0x17, 0x84, 0x6e,

    /* U+003A ":" */
    0x88,

    /* U+003B ";" */
    0x8e,

    /* U+003C "<" */
    0x9, 0xb0, 0xc1, 0x80,

    /* U+003D "=" */
    0xfc, 0xf, 0xc0,

    /* U+003E ">" */
    0x83, 0x6, 0x6c, 0x0,

    /* U+003F "?" */
    0xe1, 0x12, 0x44, 0x4,

    /* U+0040 "@" */
    0x39, 0x19, 0xe9, 0xa6, 0x99, 0xd0, 0x38,

    /* U+0041 "A" */
    0x30, 0xc3, 0x12, 0x49, 0xe4, 0xa1,

    /* U+0042 "B" */
    0xe4, 0xa5, 0xc9, 0xc6, 0x3e,

    /* U+0043 "C" */
    0x7e, 0x61, 0x8, 0x43, 0x2f,

    /* U+0044 "D" */
    0xf4, 0xa3, 0x18, 0xc6, 0x7e,

    /* U+0045 "E" */
    0xf8, 0x8f, 0x88, 0x8f,

    /* U+0046 "F" */
    0xf8, 0x8f, 0x88, 0x88,

    /* U+0047 "G" */
    0x7a, 0x61, 0x9, 0xc7, 0x2f,

    /* U+0048 "H" */
    0x8c, 0x63, 0xf8, 0xc6, 0x31,

    /* U+0049 "I" */
    0xf9, 0x8, 0x42, 0x10, 0x9f,

    /* U+004A "J" */
    0x71, 0x11, 0x11, 0x1e,

    /* U+004B "K" */
    0x8c, 0xa9, 0xca, 0x4a, 0x51,

    /* U+004C "L" */
    0x84, 0x21, 0x8, 0x42, 0x1f,

    /* U+004D "M" */
    0xde, 0xf7, 0xda, 0xc6, 0x31,

    /* U+004E "N" */
    0xce, 0x73, 0x5a, 0xce, 0x73,

    /* U+004F "O" */
    0x74, 0x63, 0x18, 0xc6, 0x2e,

    /* U+0050 "P" */
    0xf4, 0x63, 0x1f, 0x42, 0x10,

    /* U+0051 "Q" */
    0x74, 0x63, 0x18, 0xc6, 0x2e, 0x10,

    /* U+0052 "R" */
    0xe4, 0xa5, 0x2e, 0x4a, 0x51,

    /* U+0053 "S" */
    0x7c, 0x61, 0xc1, 0x84, 0x3e,

    /* U+0054 "T" */
    0xf9, 0x8, 0x42, 0x10, 0x84,

    /* U+0055 "U" */
    0x8c, 0x63, 0x18, 0xc6, 0x2e,

    /* U+0056 "V" */
    0x85, 0x24, 0x92, 0x48, 0xc3, 0xc,

    /* U+0057 "W" */
    0x86, 0x1b, 0x6d, 0xb9, 0xe4, 0x92,

    /* U+0058 "X" */
    0x49, 0x23, 0xc, 0x30, 0xa4, 0xa1,

    /* U+0059 "Y" */
    0x8c, 0x54, 0xc2, 0x10, 0x84,

    /* U+005A "Z" */
    0xf8, 0x44, 0x42, 0x22, 0x1f,

    /* U+005B "[" */
    0xea, 0xaa, 0xb0,

    /* U+005C "\\" */
    0x42, 0x8, 0x42, 0x8, 0x41, 0x8,

    /* U+005D "]" */
    0xd5, 0x55, 0x70,

    /* U+005E "^" */
    0x31, 0x24, 0x80,

    /* U+005F "_" */
    0xfc,

    /* U+0060 "`" */
    0x48,

    /* U+0061 "a" */
    0xf0, 0x5f, 0x18, 0xbc,

    /* U+0062 "b" */
    0x84, 0x3d, 0x18, 0xc6, 0x3e,

    /* U+0063 "c" */
    0x78, 0x88, 0x87,

    /* U+0064 "d" */
    0x8, 0x5f, 0x18, 0xc6, 0x2f,

    /* U+0065 "e" */
    0x74, 0x7f, 0xc, 0x3c,

    /* U+0066 "f" */
    0x74, 0xf4, 0x44, 0x44,

    /* U+0067 "g" */
    0x7c, 0x63, 0x18, 0xbc, 0x2e,

    /* U+0068 "h" */
    0x88, 0xf9, 0x99, 0x99,

    /* U+0069 "i" */
    0x20, 0x18, 0x42, 0x10, 0x9f,

    /* U+006A "j" */
    0x23, 0x92, 0x49, 0x3c,

    /* U+006B "k" */
    0x84, 0x25, 0x4c, 0x52, 0x52,

    /* U+006C "l" */
    0xc4, 0x44, 0x44, 0x47,

    /* U+006D "m" */
    0xfd, 0x6b, 0x5a, 0xd4,

    /* U+006E "n" */
    0xf9, 0x99, 0x99,

    /* U+006F "o" */
    0x74, 0x63, 0x18, 0xb8,

    /* U+0070 "p" */
    0xf4, 0x63, 0x18, 0xfa, 0x10,

    /* U+0071 "q" */
    0x7c, 0x63, 0x18, 0xbc, 0x21,

    /* U+0072 "r" */
    0xbc, 0x88, 0x88,

    /* U+0073 "s" */
    0xf8, 0xc3, 0x1e,

    /* U+0074 "t" */
    0x44, 0xf4, 0x44, 0x47,

    /* U+0075 "u" */
    0x99, 0x99, 0x9f,

    /* U+0076 "v" */
    0x49, 0x24, 0x8c, 0x30, 0xc0,

    /* U+0077 "w" */
    0x86, 0x1b, 0x9e, 0x79, 0x20,

    /* U+0078 "x" */
    0x48, 0xc3, 0xc, 0x49, 0x20,

    /* U+0079 "y" */
    0x94, 0xa4, 0xc6, 0x11, 0x18,

    /* U+007A "z" */
    0xf1, 0x24, 0x8f,

    /* U+007B "{" */
    0x32, 0x22, 0x2c, 0x22, 0x23,

    /* U+007C "|" */
    0xff, 0xe0,

    /* U+007D "}" */
    0xc4, 0x44, 0x43, 0x44, 0x4c,

    /* U+007E "~" */
    0xe1, 0xc0,

    /* U+00A0 " " */
    0x0,

    /* U+00A1 "¡" */
    0xbe,

    /* U+00A2 "¢" */
    0x22, 0x7a, 0xaa, 0xa7, 0x22,

    /* U+00A3 "£" */
    0x3a, 0x10, 0x8f, 0x21, 0x1f,

    /* U+00A4 "¤" */
    0xf, 0x99, 0xf0,

    /* U+00A5 "¥" */
    0x8c, 0x55, 0xbf, 0x90, 0x84,

    /* U+00A6 "¦" */
    0xf7, 0x80,

    /* U+00A7 "§" */
    0xf8, 0xca, 0x95, 0x31, 0xe0,

    /* U+00A8 "¨" */
    0xa0,

    /* U+00A9 "©" */
    0x79, 0x2b, 0x71, 0xc5, 0xe7, 0x80,

    /* U+00AA "ª" */
    0xe1, 0x7f, 0xf,

    /* U+00AB "«" */
    0x2a, 0xad, 0xe2, 0x80,

    /* U+00AC "¬" */
    0xf8, 0x42,

    /* U+00AD "­" */
    0xc0,

    /* U+00AE "®" */
    0x79, 0x2b, 0x6d, 0xa5, 0x27, 0x80,

    /* U+00AF "¯" */
    0xc0,

    /* U+00B0 "°" */
    0xf7, 0x80,

    /* U+00B1 "±" */
    0x21, 0x3e, 0x40, 0x7c,

    /* U+00B2 "²" */
    0xe5, 0x70,

    /* U+00B3 "³" */
    0xec, 0xf0,

    /* U+00B4 "´" */
    0x50,

    /* U+00B5 "µ" */
    0x94, 0xa5, 0x29, 0x7e, 0x10,

    /* U+00B6 "¶" */
    0x7f, 0xff, 0x73, 0x33, 0x30,

    /* U+00B7 "·" */
    0x80,

    /* U+00B8 "¸" */
    0x70,

    /* U+00B9 "¹" */
    0xc9, 0x70,

    /* U+00BA "º" */
    0x69, 0x96, 0xf,

    /* U+00BB "»" */
    0x5, 0x14, 0xaa, 0x0,

    /* U+00BC "¼" */
    0xc2, 0x11, 0xc7, 0x48, 0x46, 0x38, 0x80,

    /* U+00BD "½" */
    0xc2, 0x11, 0xc7, 0xdc, 0x22, 0x21, 0xc0,

    /* U+00BE "¾" */
    0x70, 0xc2, 0x4, 0x75, 0xe0, 0x8e, 0x3c, 0x20,

    /* U+00BF "¿" */
    0x20, 0x22, 0x48, 0x87,

    /* U+00C0 "À" */
    0x20, 0x43, 0xc, 0x31, 0x24, 0x9e, 0x4a, 0x10,

    /* U+00C1 "Á" */
    0x10, 0x83, 0xc, 0x31, 0x24, 0x9e, 0x4a, 0x10,

    /* U+00C2 "Â" */
    0x30, 0x23, 0xc, 0x31, 0x24, 0x9e, 0x4a, 0x10,

    /* U+00C3 "Ã" */
    0x38, 0x3, 0xc, 0x31, 0x24, 0x9e, 0x4a, 0x10,

    /* U+00C4 "Ä" */
    0x50, 0x8, 0xa5, 0x29, 0x5f, 0x8c, 0x40,

    /* U+00C5 "Å" */
    0x71, 0x47, 0x14, 0x51, 0x4d, 0xbe, 0x8a, 0x20,

    /* U+00C6 "Æ" */
    0x3d, 0x45, 0x17, 0x51, 0xc9, 0x27,

    /* U+00C7 "Ç" */
    0x7e, 0x61, 0x8, 0x43, 0x2e, 0x11, 0x80,

    /* U+00C8 "È" */
    0x42, 0xf8, 0x8f, 0x88, 0x8f,

    /* U+00C9 "É" */
    0x24, 0xf8, 0x8f, 0x88, 0x8f,

    /* U+00CA "Ê" */
    0x61, 0xf8, 0x8f, 0x88, 0x8f,

    /* U+00CB "Ë" */
    0x50, 0x3f, 0x8, 0x7e, 0x10, 0x87, 0xc0,

    /* U+00CC "Ì" */
    0x21, 0x3e, 0x42, 0x10, 0x84, 0x27, 0xc0,

    /* U+00CD "Í" */
    0x21, 0x3e, 0x42, 0x10, 0x84, 0x27, 0xc0,

    /* U+00CE "Î" */
    0x22, 0xbe, 0x42, 0x10, 0x84, 0x27, 0xc0,

    /* U+00CF "Ï" */
    0x50, 0x3e, 0x42, 0x10, 0x84, 0x27, 0xc0,

    /* U+00D0 "Ð" */
    0x79, 0x24, 0x79, 0x45, 0x14, 0xde,

    /* U+00D1 "Ñ" */
    0x70, 0x33, 0x9c, 0xd6, 0xb3, 0x9c, 0xc0,

    /* U+00D2 "Ò" */
    0x21, 0x1d, 0x18, 0xc6, 0x31, 0x8b, 0x80,

    /* U+00D3 "Ó" */
    0x21, 0x1d, 0x18, 0xc6, 0x31, 0x8b, 0x80,

    /* U+00D4 "Ô" */
    0x22, 0x9d, 0x18, 0xc6, 0x31, 0x8b, 0x80,

    /* U+00D5 "Õ" */
    0x70, 0x1d, 0x18, 0xc6, 0x31, 0x8b, 0x80,

    /* U+00D6 "Ö" */
    0x50, 0x1d, 0x18, 0xc6, 0x31, 0x8b, 0x80,

    /* U+00D7 "×" */
    0x9d, 0x66, 0x90,

    /* U+00D8 "Ø" */
    0x3d, 0x14, 0xd5, 0x55, 0x96, 0xee,

    /* U+00D9 "Ù" */
    0x21, 0x23, 0x18, 0xc6, 0x31, 0x8b, 0x80,

    /* U+00DA "Ú" */
    0x21, 0x23, 0x18, 0xc6, 0x31, 0x8b, 0x80,

    /* U+00DB "Û" */
    0x22, 0xa3, 0x18, 0xc6, 0x31, 0x8b, 0x80,

    /* U+00DC "Ü" */
    0x50, 0x23, 0x18, 0xc6, 0x31, 0x8b, 0x80,

    /* U+00DD "Ý" */
    0x21, 0x23, 0x15, 0x30, 0x84, 0x21, 0x0,

    /* U+00DE "Þ" */
    0x84, 0x3d, 0x18, 0xfa, 0x10,

    /* U+00DF "ß" */
    0x74, 0xa5, 0x4b, 0x46, 0x36,

    /* U+00E0 "à" */
    0x41, 0x1, 0xe0, 0xbe, 0x31, 0x78,

    /* U+00E1 "á" */
    0x10, 0x89, 0xe0, 0xbe, 0x31, 0x78,

    /* U+00E2 "â" */
    0x22, 0x91, 0xe0, 0xbe, 0x31, 0x78,

    /* U+00E3 "ã" */
    0x70, 0x3c, 0x17, 0xc6, 0x2f,

    /* U+00E4 "ä" */
    0x50, 0x3c, 0x17, 0xc6, 0x2f,

    /* U+00E5 "å" */
    0x22, 0x94, 0xcf, 0x5, 0xf1, 0x8b, 0xc0,

    /* U+00E6 "æ" */
    0xd9, 0x7f, 0x4a, 0x6c,

    /* U+00E7 "ç" */
    0x78, 0x88, 0x87, 0x22,

    /* U+00E8 "è" */
    0x41, 0x0, 0xe8, 0xfe, 0x18, 0x78,

    /* U+00E9 "é" */
    0x11, 0x0, 0xe8, 0xfe, 0x18, 0x78,

    /* U+00EA "ê" */
    0x22, 0x80, 0xe8, 0xfe, 0x18, 0x78,

    /* U+00EB "ë" */
    0x50, 0x1d, 0x1f, 0xc3, 0xf,

    /* U+00EC "ì" */
    0x42, 0x8, 0xc2, 0x10, 0x84, 0xf8,

    /* U+00ED "í" */
    0x11, 0x0, 0xc2, 0x10, 0x84, 0xf8,

    /* U+00EE "î" */
    0x23, 0x4, 0xc2, 0x10, 0x84, 0xf8,

    /* U+00EF "ï" */
    0x50, 0x18, 0x42, 0x10, 0x9f,

    /* U+00F0 "ð" */
    0x71, 0x1d, 0x18, 0xc6, 0x2e,

    /* U+00F1 "ñ" */
    0x60, 0xf9, 0x99, 0x99,

    /* U+00F2 "ò" */
    0x41, 0x0, 0xe8, 0xc6, 0x31, 0x70,

    /* U+00F3 "ó" */
    0x11, 0x0, 0xe8, 0xc6, 0x31, 0x70,

    /* U+00F4 "ô" */
    0x22, 0x80, 0xe8, 0xc6, 0x31, 0x70,

    /* U+00F5 "õ" */
    0x70, 0x1d, 0x18, 0xc6, 0x2e,

    /* U+00F6 "ö" */
    0x50, 0x1d, 0x18, 0xc6, 0x2e,

    /* U+00F7 "÷" */
    0x20, 0x3e, 0x2, 0x0,

    /* U+00F8 "ø" */
    0x7c, 0xeb, 0x9c, 0xf8,

    /* U+00F9 "ù" */
    0x84, 0x29, 0x99, 0x99, 0xf0,

    /* U+00FA "ú" */
    0x2, 0x49, 0x99, 0x99, 0xf0,

    /* U+00FB "û" */
    0x6, 0x9, 0x99, 0x99, 0xf0,

    /* U+00FC "ü" */
    0x50, 0x99, 0x99, 0x9f,

    /* U+00FD "ý" */
    0x22, 0x1, 0x29, 0x49, 0x8c, 0x22, 0x30,

    /* U+00FE "þ" */
    0x84, 0x3d, 0x18, 0xc6, 0x3e, 0x84, 0x0,

    /* U+00FF "ÿ" */
    0x50, 0x23, 0x15, 0x28, 0xc4, 0x26, 0x0,

    /* U+2014 "—" */
    0xfc,

    /* U+2022 "•" */
    0xff, 0x80,

    /* U+2026 "…" */
    0xa8,

    /* U+2190 "←" */
    0x87, 0xe0,

    /* U+2191 "↑" */
    0x5d, 0x24,

    /* U+2192 "→" */
    0x17, 0xc4,

    /* U+2193 "↓" */
    0x49, 0x2e,

    /* U+2194 "↔" */
    0x97, 0xe4,

    /* U+2195 "↕" */
    0x5d, 0x2e,

    /* U+2261 "≡" */
    0xf8, 0x3f, 0xf0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 96, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 96, .box_w = 1, .box_h = 8, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 2, .adv_w = 96, .box_w = 3, .box_h = 3, .ofs_x = 2, .ofs_y = 5},
    {.bitmap_index = 4, .adv_w = 96, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 10, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 17, .adv_w = 96, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 23, .adv_w = 96, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 29, .adv_w = 96, .box_w = 1, .box_h = 3, .ofs_x = 3, .ofs_y = 5},
    {.bitmap_index = 30, .adv_w = 96, .box_w = 2, .box_h = 10, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 33, .adv_w = 96, .box_w = 2, .box_h = 10, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 36, .adv_w = 96, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 40, .adv_w = 96, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 44, .adv_w = 96, .box_w = 1, .box_h = 3, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 45, .adv_w = 96, .box_w = 2, .box_h = 1, .ofs_x = 2, .ofs_y = 2},
    {.bitmap_index = 46, .adv_w = 96, .box_w = 1, .box_h = 1, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 47, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 53, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 58, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 63, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 68, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 73, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 78, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 83, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 88, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 93, .adv_w = 96, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 97, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 102, .adv_w = 96, .box_w = 1, .box_h = 5, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 103, .adv_w = 96, .box_w = 1, .box_h = 7, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 104, .adv_w = 96, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 108, .adv_w = 96, .box_w = 6, .box_h = 3, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 111, .adv_w = 96, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 115, .adv_w = 96, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 119, .adv_w = 96, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 126, .adv_w = 96, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 132, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 137, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 142, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 147, .adv_w = 96, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 151, .adv_w = 96, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 155, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 160, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 165, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 170, .adv_w = 96, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 174, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 179, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 184, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 189, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 194, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 199, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 204, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 210, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 215, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 220, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 225, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 230, .adv_w = 96, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 236, .adv_w = 96, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 242, .adv_w = 96, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 248, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 253, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 258, .adv_w = 96, .box_w = 2, .box_h = 10, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 261, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 267, .adv_w = 96, .box_w = 2, .box_h = 10, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 270, .adv_w = 96, .box_w = 6, .box_h = 3, .ofs_x = 0, .ofs_y = 5},
    {.bitmap_index = 273, .adv_w = 96, .box_w = 6, .box_h = 1, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 274, .adv_w = 96, .box_w = 3, .box_h = 2, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 275, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 279, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 284, .adv_w = 96, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 287, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 292, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 296, .adv_w = 96, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 300, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 305, .adv_w = 96, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 309, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 314, .adv_w = 96, .box_w = 3, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 318, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 323, .adv_w = 96, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 327, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 331, .adv_w = 96, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 334, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 338, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 343, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 348, .adv_w = 96, .box_w = 4, .box_h = 6, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 351, .adv_w = 96, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 354, .adv_w = 96, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 358, .adv_w = 96, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 361, .adv_w = 96, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 366, .adv_w = 96, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 371, .adv_w = 96, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 376, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 381, .adv_w = 96, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 384, .adv_w = 96, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 389, .adv_w = 96, .box_w = 1, .box_h = 11, .ofs_x = 3, .ofs_y = -3},
    {.bitmap_index = 391, .adv_w = 96, .box_w = 4, .box_h = 10, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 396, .adv_w = 96, .box_w = 5, .box_h = 2, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 398, .adv_w = 96, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 399, .adv_w = 96, .box_w = 1, .box_h = 7, .ofs_x = 3, .ofs_y = -1},
    {.bitmap_index = 400, .adv_w = 96, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 405, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 410, .adv_w = 96, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 413, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 418, .adv_w = 96, .box_w = 1, .box_h = 9, .ofs_x = 3, .ofs_y = -2},
    {.bitmap_index = 420, .adv_w = 96, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 425, .adv_w = 96, .box_w = 3, .box_h = 1, .ofs_x = 2, .ofs_y = 7},
    {.bitmap_index = 426, .adv_w = 96, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 432, .adv_w = 96, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 435, .adv_w = 96, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 439, .adv_w = 96, .box_w = 5, .box_h = 3, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 441, .adv_w = 96, .box_w = 2, .box_h = 1, .ofs_x = 2, .ofs_y = 2},
    {.bitmap_index = 442, .adv_w = 96, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 448, .adv_w = 96, .box_w = 2, .box_h = 1, .ofs_x = 2, .ofs_y = 7},
    {.bitmap_index = 449, .adv_w = 96, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 451, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 455, .adv_w = 96, .box_w = 3, .box_h = 4, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 457, .adv_w = 96, .box_w = 3, .box_h = 4, .ofs_x = 2, .ofs_y = 4},
    {.bitmap_index = 459, .adv_w = 96, .box_w = 3, .box_h = 2, .ofs_x = 2, .ofs_y = 7},
    {.bitmap_index = 460, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 465, .adv_w = 96, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 470, .adv_w = 96, .box_w = 1, .box_h = 1, .ofs_x = 2, .ofs_y = 3},
    {.bitmap_index = 471, .adv_w = 96, .box_w = 2, .box_h = 2, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 472, .adv_w = 96, .box_w = 3, .box_h = 4, .ofs_x = 2, .ofs_y = 4},
    {.bitmap_index = 474, .adv_w = 96, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 477, .adv_w = 96, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 481, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 488, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 495, .adv_w = 96, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 503, .adv_w = 96, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 507, .adv_w = 96, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 515, .adv_w = 96, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 523, .adv_w = 96, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 531, .adv_w = 96, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 539, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 546, .adv_w = 96, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 554, .adv_w = 96, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 560, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 567, .adv_w = 96, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 572, .adv_w = 96, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 577, .adv_w = 96, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 582, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 589, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 596, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 603, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 610, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 617, .adv_w = 96, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 623, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 630, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 637, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 644, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 651, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 658, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 665, .adv_w = 96, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 668, .adv_w = 96, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 674, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 681, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 688, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 695, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 702, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 709, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 714, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 719, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 725, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 731, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 737, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 742, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 747, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 754, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 758, .adv_w = 96, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 762, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 768, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 774, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 780, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 785, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 791, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 797, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 803, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 808, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 813, .adv_w = 96, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 817, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 823, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 829, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 835, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 840, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 845, .adv_w = 96, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 849, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 853, .adv_w = 96, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 858, .adv_w = 96, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 863, .adv_w = 96, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 868, .adv_w = 96, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 872, .adv_w = 96, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 879, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 886, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 893, .adv_w = 96, .box_w = 6, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 894, .adv_w = 96, .box_w = 3, .box_h = 3, .ofs_x = 2, .ofs_y = 2},
    {.bitmap_index = 896, .adv_w = 96, .box_w = 5, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 897, .adv_w = 96, .box_w = 5, .box_h = 3, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 899, .adv_w = 96, .box_w = 3, .box_h = 5, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 901, .adv_w = 96, .box_w = 5, .box_h = 3, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 903, .adv_w = 96, .box_w = 3, .box_h = 5, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 905, .adv_w = 96, .box_w = 5, .box_h = 3, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 907, .adv_w = 96, .box_w = 3, .box_h = 5, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 909, .adv_w = 96, .box_w = 5, .box_h = 4, .ofs_x = 1, .ofs_y = 1}
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
const lv_font_t phos_mono_18_cs = {
#else
lv_font_t phos_mono_18_cs = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 13,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc           /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
};



#endif /*#if PHOS_MONO_18_CS*/

