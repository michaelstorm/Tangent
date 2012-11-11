#ifndef GRID_H
#define GRID_H

#include <stdio.h>

#ifndef PI
#define PI ((long double)3.14159265358979323)
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct grid
{
	int cols;
	int rows;
	char **cells;
};

struct circle
{
	long double center_x;
	long double center_y;
	long double diameter;
	long double rotation;
	long double row_ratio;
	long double density;
};

struct line
{
	long double from_x;
	long double from_y;
	long double domain;
	long double range;
	long double row_ratio;
};

typedef long double (*parametric_equation)(void *arg, long double input);

struct grid *new_grid(int cols, int rows);
void free_grid(struct grid *g);
void print_grid(FILE *fp, struct grid *g);

struct line *new_line(long double from_x, long double from_y, long double to_x,
					  long double to_y, long double row_ratio);
void free_line(struct line *l);

void draw_butterfly(struct grid *g, struct circle *c, char marker,
					long double from, long double to);

struct circle *new_circle(long double center_x, long double center_y,
						  long double diameter,long double rotation,
						  long double row_ratio, long double density);
void free_circle(struct circle *c);

void draw_circle(struct grid *g, struct circle *c, char marker);
void draw_arc(struct grid *g, struct circle *c, char marker,
			  long double from_rad, long double to_rad);
void draw_radius_automarker(struct grid *g, struct circle *c,
							long double to_rad);
void draw_radius(struct grid *g, struct circle *c, char marker,
				 long double to_rad);
void draw_chord(struct grid *g, struct circle *c, char marker,
				 long double from_rad, long double to_rad);
void draw_circle_point(struct grid *g, struct circle *c, char marker,
					   long double rad);

void draw_marker(struct grid *g, int marker, int col, int row);
void draw_parametric(struct grid *g, void *arg, char marker, int points,
					 parametric_equation x_eq, parametric_equation y_eq,
					 long double from, long double to);

int measure_text(const char *text, int *width, int *height);
void draw_box(struct grid *g, int x, int y, int width, int height);
void draw_text(struct grid *g, const char *text, int x, int y, int in_box);
void draw_centered_text(struct grid *g, const char *text, int x, int y,
						int in_box);

#ifdef __cplusplus
}
#endif

#endif
