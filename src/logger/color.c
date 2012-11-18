#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include "chord/logger/color.h"

#define ATTR_CODE_OFF        0
#define ATTR_CODE_BOLD       1
#define ATTR_CODE_UNDERSCORE 4
#define ATTR_CODE_BLINK      5
#define ATTR_CODE_REVERSE    7
#define ATTR_CODE_CONCEALED  8

static int FG_COLORS[]    = { 30, 31, 32, 33, 34, 35, 36, 37 };
static int BG_COLORS[]    = { 40, 41, 42, 43, 44, 45, 46, 47 };
static int FG_HI_COLORS[] = { 90, 91, 92, 93, 94, 95, 96, 97 };
static int BG_HI_COLORS[] = { 100, 101, 102, 103, 104, 105, 106, 107 };

static int get_attr_code(int attr)
{
	switch (attr) {
		case ATTR_OFF:        return ATTR_CODE_OFF;
		case ATTR_BOLD:       return ATTR_CODE_BOLD;
		case ATTR_UNDERSCORE: return ATTR_CODE_UNDERSCORE;
		case ATTR_BLINK:      return ATTR_CODE_BLINK;
		case ATTR_REVERSE:    return ATTR_CODE_REVERSE;
		case ATTR_CONCEALED:  return ATTR_CODE_CONCEALED;
	}
	
	return ATTR_CODE_OFF;
}

void start_color(FILE *stream, int color)
{
	int attr      = color & 0x000000FF;
	int fg        = (color & 0x0000FF00) >>  8;
	int bg        = (color & 0x00FF0000) >> 16;
	int color_mod = color & 0xFF000000;
	
	int i;
	for (i = 0; i < 6; i++) {
		int single_attr = attr & (1 << i);
		if (single_attr != ATTR_DEFAULT) {
			int attr_code = get_attr_code(single_attr);
			fprintf(stream, "%c[%dm", 0x1B, attr_code);
		}
	}
	
	if (fg != FG_DEFAULT) {
		int fg_index = fg - 1;
		int fg_color = color_mod & MOD_INTENSE_FG ? FG_HI_COLORS[fg_index] : FG_COLORS[fg_index];
		fprintf(stream, "%c[%dm", 0x1B, fg_color);
	}
	
	if (bg != BG_DEFAULT) {
		int bg_index = bg - 1;
		int bg_color = color_mod & MOD_INTENSE_BG ? BG_HI_COLORS[bg_index] : BG_COLORS[bg_index];
		fprintf(stream, "%c[%dm", 0x1B, bg_color);
	}
}

void default_color(FILE *stream)
{
	fprintf(stream, "%c[0m", 0x1B);
}

int cfprintf(FILE *stream, int color, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int ret = cvfprintf(stream, color, fmt, args);
	va_end(args);
	
	return ret;
}

int cvfprintf(FILE *stream, int color, const char *fmt, va_list args)
{
	int tty = isatty(fileno(stream));
	
	if (tty)
		start_color(stream, color);

	int ret = vfprintf(stream, fmt, args);

	if (tty)
		default_color(stream);

	return ret;
}