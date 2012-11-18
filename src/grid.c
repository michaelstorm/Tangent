#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "chord/grid.h"

struct grid *new_grid(int cols, int rows)
{
	struct grid *g = malloc(sizeof(struct grid));
	g->cols = cols;
	g->rows = rows;
	g->cells = malloc(sizeof(char *)*g->cols);

	int i, j;
	for (i = 0; i < g->cols; i++) {
		g->cells[i] = malloc(sizeof(char)*g->rows);
		for (j = 0; j < g->rows; j++)
			g->cells[i][j] = ' ';
	}

	return g;
}

void free_grid(struct grid *g)
{
	if (g) {
		int i;
		for (i = 0; i < g->cols; i++)
			free(g->cells[i]);

		free(g->cells);
		free(g);
	}
}

void print_grid(FILE *fp, struct grid *g)
{
	int row;
	int col;

	for (row = 0; row < g->rows; row++) {
		for (col = 0; col < g->cols; col++)
			fprintf(fp, "%c", g->cells[col][row]);
		fprintf(fp, "\n");
	}
}

struct line *new_line(long double from_x, long double from_y, long double to_x,
					  long double to_y, long double row_ratio)
{
	struct line *l = malloc(sizeof(struct line));
	l->from_x = from_x;
	l->from_y = from_y;
	l->domain = to_x - from_x;
	l->range = to_y - from_y;
	l->row_ratio = row_ratio;
	return l;
}

void free_line(struct line *l)
{
	free(l);
}

static long double get_line_x(void *arg, long double input)
{
	struct line *l = arg;
	return l->from_x+(l->domain*input);
}

static long double get_actual_y(void *arg, long double input)
{
	struct line *l = arg;
	return l->from_y+(l->range*input);
}

static long double get_line_y(void *arg, long double input)
{
	struct line *l = arg;
	return get_actual_y(l, input)*l->row_ratio;
}

static long double get_butterfly_x(void *arg, long double input)
{
	struct circle *c = arg;
	return c->center_x+(sinl(input)*(expl(cosl(input))-(2*cosl(4*input))
									 -powl(sinl(input/12), 5))
						*(c->diameter/7.3));
}

static long double get_butterfly_actual_y(void *arg, long double input)
{
	struct circle *c = arg;
	return (c->center_y/1.3)+(cosl(input)*(expl(cosl(input))-(2*cosl(4*input))
									 -powl(sinl(input/12), 5))
							  *(c->diameter/7.3));
}

static long double get_butterfly_y(void *arg, long double input)
{
	struct circle *c = arg;
	return get_butterfly_actual_y(arg, input)*c->row_ratio;
}

void draw_butterfly(struct grid *g, struct circle *c, char marker,
					long double from, long double to)
{
	draw_parametric(g, c, marker, powl(c->diameter, 2)*c->density,
					get_butterfly_x, get_butterfly_y, from, to);
}

struct circle *new_circle(long double center_x, long double center_y,
						  long double diameter,long double rotation,
						  long double row_ratio, long double density)
{
	struct circle *c = malloc(sizeof(struct circle));
	c->center_x = center_x;
	c->center_y = center_y;
	c->diameter = diameter;
	c->rotation = rotation;
	c->row_ratio = row_ratio;
	c->density = density;
	return c;
}

void free_circle(struct circle *c)
{
	free(c);
}

void draw_circle(struct grid *g, struct circle *c, char marker)
{
	draw_arc(g, c, marker, 0, 2*PI);
}

static long double get_circle_x(void *arg, long double input)
{
	struct circle *c = arg;
	return c->center_x+(cosl(input+c->rotation)*(c->diameter/2));
}

static long double get_circle_actual_y(void *arg, long double input)
{
	struct circle *c = arg;
	return c->center_y+(sinl(input+c->rotation)*(c->diameter/2));
}

static long double get_circle_y(void *arg, long double input)
{
	struct circle *c = arg;
	return get_circle_actual_y(c, input)*c->row_ratio;
}

void draw_arc(struct grid *g, struct circle *c, char marker,
			  long double from_rad, long double to_rad)
{
	// ensure that range is positive
	while (to_rad < from_rad)
		to_rad += 2*PI;

	// circumference * ratio of arc length to full revolution * density
	int points = PI*c->diameter*(to_rad-from_rad)/(2*PI)*c->density;
	draw_parametric(g, c, marker, points, get_circle_x, get_circle_y,
					from_rad, to_rad);
}

long double normalize_radians(long double rad)
{
	// wrap in if-blocks to prevent infinite loop in weird rounding cases
	if (rad >= 2*PI) {
		while (rad > 2*PI)
			rad -= 2*PI;
	}
	else if (rad < 0) {
		while (rad < 0)
			rad += 2*PI;
	}

	if (rad < 0)
		rad = 0;
	else if (rad >= 2*PI)
		rad = (2*PI)-powl(2, -11);
	return rad;
}

void draw_radius_automarker(struct grid *g, struct circle *c,
							long double to_rad)
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

	draw_radius(g, c, marker, to_rad);
}

void draw_radius(struct grid *g, struct circle *c, char marker,
				 long double to_rad)
{
	struct line *l = new_line(c->center_x, c->center_y,
							  get_circle_x(c, to_rad),
							  get_circle_actual_y(c, to_rad), c->row_ratio);

	draw_parametric(g, l, marker, ((c->diameter)/2)*c->density, get_line_x,
					get_line_y, 0, 1);

	free_line(l);
}

void draw_chord(struct grid *g, struct circle *c, char marker,
				 long double from_rad, long double to_rad)
{
	struct line *l = new_line(get_circle_x(c, from_rad),
							  get_circle_actual_y(c, from_rad),
							  get_circle_x(c, to_rad),
							  get_circle_actual_y(c, to_rad), c->row_ratio);

	draw_parametric(g, l, marker, c->diameter*c->density, get_line_x,
					get_line_y, 0, 1);

	free_line(l);
}

void draw_circle_point(struct grid *g, struct circle *c, char marker,
					   long double rad)
{
	draw_parametric(g, c, marker, 1, get_circle_x, get_circle_y, rad, rad);
}

void draw_marker(struct grid *g, int marker, int col, int row)
{
	if (row >= 0 && col >= 0 && row < g->rows && col < g->cols)
		g->cells[col][row] = marker;
}

void draw_parametric(struct grid *g, void *arg, char marker, int points,
					 parametric_equation x_eq, parametric_equation y_eq,
					 long double from, long double to)
{
	long double range = fabsl(to - from);

	int i;
	long double current = from;
	long double denom = points > 1 ? points-1 : 1;
	for (i = 0; i < points; i++) {
		current = from+(range*(((long double)i)/denom));
		int row = g->rows-1 - lrintl(y_eq(arg, current));
		int col = lrintl(x_eq(arg, current));
		draw_marker(g, marker, col, row);
	}
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

void draw_box(struct grid *g, int x, int y, int width, int height)
{
	y = g->rows-1 - y;

	int i;
	draw_marker(g, '+', x, y);
	for (i = 1; i < width-1; i++)
		draw_marker(g, '-', x+i, y);
	draw_marker(g, '+', x+width-1, y);

	for (i = 1; i < height-1; i++) {
		draw_marker(g, '|', x, y+i);
		draw_marker(g, '|', x+width-1, y+i);
	}

	draw_marker(g, '+', x, y+height-1);
	for (i = 1; i < width-1; i++)
		draw_marker(g, '-', x+i, y+height-1);
	draw_marker(g, '+', x+width-1, y+height-1);
}

void draw_text(struct grid *g, const char *text, int x, int y, int in_box)
{
	int i;
	int len = strlen(text);
	int margin = in_box ? 1 : 0;

	int orig_y = y;
	y = g->rows-1 - y;
	int row = y+margin;
	int col = x+margin;

	for (i = 0; i < len; i++) {
		if (text[i] == '\n') {
			col = x+margin;
			row++;
		}
		else {
			draw_marker(g, text[i], col, row);
			col++;
		}
	}

	if (in_box) {
		int text_width, text_height;
		measure_text(text, &text_width, &text_height);
		draw_box(g, x, orig_y, text_width+2, text_height+2);
	}
}

void draw_centered_text(struct grid *g, const char *text, int x, int y,
						int in_box)
{
	int i;
	int text_width = 0;
	int text_height = 0;
	int len = measure_text(text, &text_width, &text_height);

	int line_begin = 0;
	int line_num = 0;
	int margin = in_box ? 1 : 0;
	int orig_y = y;
	y = g->rows-1 - y;

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
					draw_marker(g, ' ', x+j+margin, y+line_num+margin);
			}

			for (j = 0; j < line_length; j++)
				draw_marker(g, text[line_begin+j],
							x+(text_width-line_length)/2+margin+j,
							y+line_num+margin);

			line_begin = i+1;
			line_num++;
		}
	}

	if (in_box)
		draw_box(g, x, orig_y, text_width+2, text_height+2);
}
