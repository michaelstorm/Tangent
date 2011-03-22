#include <stdlib.h>
#include <math.h>
#include "chord.h"
#include "circle.h"

struct circle *new_circle(int diam, int dens, long double rot)
{
	struct circle *c = malloc(sizeof(struct circle));
	c->diameter = diam;
	c->row_count = c->diameter*ROW_RATIO;
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

long double get_x(struct circle *c, long double rad)
{
	return cos(rad)*(c->diameter-1)/2 + c->diameter/2;
}

long double get_y(struct circle *c, long double rad)
{
	return sin(rad)*(c->diameter-1)/2 + c->diameter/2;
}

void draw_arc(struct circle *c, char marker, long double from_rad,
			  long double to_rad)
{
	from_rad += c->rotation;
	to_rad += c->rotation;

	int points = PI*c->diameter*c->density;
	long double rad_range = to_rad > from_rad ? to_rad - from_rad
											  : 2*PI - (from_rad-to_rad);

	int i;
	for (i = 0; i < points; i++) {
		long double rad = rad_range*i/points+from_rad;
		int row = (int)(get_y(c, rad)*ROW_RATIO);
		int col = (int)get_x(c, rad);
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
	long double radius = ((long double)c->diameter)/2;
	long double domain = cos(to_rad)*radius;
	long double slope = sin(to_rad)/cos(to_rad);
	int points = radius*c->density;

	if (!isfinite(slope)) {
		to_rad = normalize_radians(to_rad);
		int mult = 1;
		if (fabs(to_rad - 3*PI/2) < fabs(to_rad - PI/2))
			mult = -1;

		int i;
		int col = radius;
		for (i = 0; i < points; i++) {
			long double y = radius+((long double)i)/points*radius*mult;
			int row = y*ROW_RATIO;
			c->rows[row][col] = marker;
		}
	}
	else {
		int i;
		for (i = 0; i < points; i++) {
			long double x = radius+((long double)i)/points*domain;
			long double y = radius+((long double)i)/points*domain*slope;

			int col = x;
			int row = y*ROW_RATIO;
			c->rows[row][col] = marker;
		}
	}
}

void draw_point(struct circle *c, char marker, long double rad)
{
	rad += c->rotation;
	int row = (int)(get_y(c, rad)*ROW_RATIO);
	int col = (int)get_x(c, rad);
	c->rows[row][col] = marker;
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
