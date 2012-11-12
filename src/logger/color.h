#ifndef COLOR_H
#define COLOR_H

#define ATTR_DEFAULT     0x00000000
#define ATTR_OFF         0x00000001
#define ATTR_BOLD        0x00000002
#define ATTR_UNDERSCORE  0x00000004
#define ATTR_BLINK       0x00000008
#define ATTR_REVERSE     0x00000010
#define ATTR_CONCEALED   0x00000020

#define FG_DEFAULT       0x00000000
#define FG_BLACK         0x00000100
#define FG_RED           0x00000200
#define FG_GREEN         0x00000300
#define FG_YELLOW        0x00000400
#define FG_BLUE          0x00000500
#define FG_PURPLE        0x00000600
#define FG_CYAN          0x00000700
#define FG_WHITE         0x00000800

#define BG_DEFAULT       0x00000000
#define BG_BLACK         0x00010000
#define BG_RED           0x00020000
#define BG_GREEN         0x00030000
#define BG_YELLOW        0x00040000
#define BG_BLUE          0x00050000
#define BG_PURPLE        0x00060000
#define BG_CYAN          0x00070000
#define BG_WHITE         0x00080000

#define COLOR_DEFAULT    0x00000000
#define COLOR_INTENSE_FG 0x01000000
#define COLOR_INTENSE_BG 0x02000000

#ifdef __cplusplus
extern "C" {
#endif

void start_color(FILE *stream, int color);
void default_color(FILE *stream);
int  cfprintf(FILE *stream, int color, const char *fmt, ...);
int  cvfprintf(FILE *stream, int color, const char *fmt, va_list args);

#ifdef __cplusplus
}
#endif

#endif