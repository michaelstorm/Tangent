#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "chord.h"
#include "circle.h"

struct circle *new_circle(long double diam, int dens, long double rot)
{
	struct circle *c = malloc(sizeof(struct circle));
	c->diameter = diam;
	c->row_count = (c->diameter*ROW_RATIO)+1;
	c->density = dens;
	c->rotation = rot;

	c->rows = malloc(sizeof(char *)*c->row_count);

	int i, j;
	for (i = 0; i < c->row_count; i++) {
		c->rows[i] = malloc(sizeof(char)*c->diameter);
		for (j = 0; j < c->diameter; j++)
			c->rows[i][j] = ' ';
	}

	return c;
}

void free_circle(struct circle *c)
{
	int i;
	for (i = 0; i < c->row_count; i++)
		free(c->rows[i]);

	free(c->rows);
	free(c);
}

void draw_circle(struct circle *c, char marker)
{
	draw_arc(c, marker, 0, 2*PI);
}

long double get_x_coord(struct circle *c, long double rad)
{
	return (cos(rad)+1)*((c->diameter-1)/2);
}

long double get_y_coord(struct circle *c, long double rad)
{
	return (sin(rad)+1)*((c->diameter-1)/2);
}

void draw_arc(struct circle *c, char marker, long double from_rad,
			  long double to_rad)
{
	from_rad += c->rotation;
	to_rad += c->rotation;

	long double rad_range = to_rad > from_rad ? to_rad - from_rad
											  : 2*PI - (from_rad-to_rad);

	// circumference * ratio of arc length to full revolution * density
	int points = PI*c->diameter*rad_range/(2*PI)*c->density;

	int i;
	for (i = 0; i < points; i++) {
		long double rad = (rad_range*((long double)i)/points)+from_rad;
		int row = lrintl(get_y_coord(c, rad)*ROW_RATIO);
		int col = lrintl(get_x_coord(c, rad));
		c->rows[row][col] = marker;
	}
}

long double normalize_radians(long double rad)
{
	// wrap in if-blocks to prevent infinite loop in weird rounding cases
	if (rad > 2*PI) {
		while (rad > 2*PI)
			rad -= 2*PI;
	}
	else if (rad < 0) {
		while (rad < 0)
			rad += 2*PI;
	}
	return rad;
}

void draw_radius_automarker(struct circle *c, long double to_rad)
{
	char marker;
	long double norm_rad = normalize_radians(to_rad + c->rotation);

	if (norm_rad < PI) {
		if (norm_rad < PI/8)
			marker = '-';
		else if (norm_rad < 3*PI/8)
			marker = '\\';
		else if (norm_rad < 5*PI/8)
			marker = '|';
		else if (norm_rad < 7*PI/8)
			marker = '/';
		else
			marker = '-';
	}
	else {
		if (norm_rad < 9*PI/8)
			marker = '-';
		else if (norm_rad < 11*PI/8)
			marker = '\\';
		else if (norm_rad < 13*PI/8)
			marker = '|';
		else if (norm_rad < 15*PI/8)
			marker = '/';
		else
			marker = '-';
	}

	draw_radius(c, marker, to_rad);
}

void draw_radius(struct circle *c, char marker, long double to_rad)
{
	to_rad += c->rotation;
	long double radius = c->diameter/2;
	long double domain = cos(to_rad)*radius;
	long double slope = sin(to_rad)/cos(to_rad);
	int points = radius*c->density;

	if (isfinite(slope)) {
		int i;
		for (i = 0; i < points; i++) {
			long double x = radius+((long double)i)/points*domain;
			long double y = radius+((long double)i)/points*domain*slope;

			int col = lrintl(x);
			int row = lrintl(y*ROW_RATIO);
			c->rows[row][col] = marker;
		}
	}
	else {
		to_rad = normalize_radians(to_rad);
		int mult = 1;
		if (fabs(to_rad - 3*PI/2) < fabs(to_rad - PI/2))
			mult = -1;

		int i;
		for (i = 0; i < points; i++) {
			long double y = radius+((long double)i)/points*radius*mult;

			int col = lrintl(radius);
			int row = lrintl(y*ROW_RATIO);
			c->rows[row][col] = marker;
		}
	}
}

void draw_point(struct circle *c, char marker, long double rad)
{
	rad += c->rotation;
	int row = lrintl(get_y_coord(c, rad)*ROW_RATIO);
	int col = lrintl(get_x_coord(c, rad));
	c->rows[row][col] = marker;
}

void draw_box(struct circle *c, int x, int y, int width, int height)
{
	int i;

	c->rows[y][x] = '+';
	for (i = 1; i < width-1; i++)
		c->rows[y][x+i] = '-';
	c->rows[y][x+width-1] = '+';

	for (i = 1; i < height-1; i++) {
		c->rows[y+i][x] = '|';
		c->rows[y+i][x+width-1] = '|';
	}

	c->rows[y+height-1][x] = '+';
	for (i = 1; i < width-1; i++)
		c->rows[y+height-1][x+i] = '-';
	c->rows[y+height-1][x+width-1] = '+';
}

int measure_text(const char *text, int *width, int *height)
{
	int i;
	int this_line = 0;
	int longest_line = 0;
	int line_num = 0;

	for (i = 0; text[i] != '\0'; i++) {
		if (text[i] == '\n' || text[i+1] == '\0') {
			this_line++;
			if (this_line > longest_line)
				longest_line = this_line;
			this_line = 0;
			line_num++;
		}
		else
			this_line++;
	}

	if (width)
		*width = longest_line;
	if (height)
		*height = line_num;
	return i;
}

void draw_text(struct circle *c, const char *text, int x, int y, int in_box)
{
	int i;
	int len = strlen(text);
	int margin = in_box ? 1 : 0;
	int row = y+margin;
	int col = x+margin;

	for (i = 0; i < len; i++) {
		if (text[i] == '\n') {
			col = x+margin;
			row++;
		}
		else {
			c->rows[row][col] = text[i];
			col++;
		}
	}

	if (in_box) {
		int text_width, text_height;
		measure_text(text, &text_width, &text_height);
		draw_box(c, x, y, text_width+2, text_height+2);
	}
}

void draw_centered_text(struct circle *c, const char *text, int x, int y,
						int in_box)
{
	int i;
	int text_width = 0;
	int text_height = 0;
	int len = measure_text(text, &text_width, &text_height);

	int line_begin = 0;
	int line_num = 0;
	int margin = in_box ? 1 : 0;
	for (i = 0; i < len; i++) {
		if (text[i] == '\n' || i == len-1) {
			int line_length = i - line_begin;
			if (i == len-1)
				line_length++;

			char line[line_length+1];
			memcpy(line, text+line_begin, line_length);
			line[line_length] = '\0';

			int j;
			if (in_box) {
				for (j = 0; j < text_width; j++)
					c->rows[y+line_num+margin][x+j+margin] = ' ';
			}

			for (j = 0; j < line_length; j++)
				c->rows[y+line_num+margin][x+(text_width-line_length)/2+margin+j]
						= text[line_begin+j];

			line_begin = i+1;
			line_num++;
		}
	}

	if (in_box)
		draw_box(c, x, y, text_width+2, text_height+2);
}

void print_circle(FILE *fp, struct circle *c)
{
	int row;
	int col;

	for (row = 0; row < c->row_count; row++) {
		for (col = 0; col < c->diameter; col++)
			fprintf(fp, "%c", c->rows[row][col]);
		fprintf(fp, "\n");
	}
}

long double id_to_radians(const chordID *id)
{
	int i;
	long double rad = 0.0;

	for (i = 0; i < CHORD_ID_LEN; i++) {
		long double numerator = id->x[i];
		long double denominator = pow((long double)256, (long double)(i+1));
		rad += numerator/denominator;
	}
	return rad*2*PI;
}
