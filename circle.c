#include <stdlib.h>
#include <math.h>
#include "chord.h"
#include "circle.h"

struct circle *new_circle(int diam, int dens)
{
	struct circle *c = (struct circle *)malloc(sizeof(struct circle));
	c->diameter = diam;
	c->row_count = c->diameter*ROW_RATIO;
	c->density = dens;

	c->rows = (char **)malloc(sizeof(char *)*c->row_count);

	int i, j;
	for (i = 0; i < c->row_count; i++) {
		c->rows[i] = (char *)malloc(sizeof(char)*c->diameter);
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

void draw_circle(struct circle *c, char marker, long double from_rad,
				   long double to_rad)
{
	int i;
	int points = c->diameter*c->density;
	long double rad_range = to_rad > from_rad ? to_rad - from_rad
											  : 2*PI - (from_rad-to_rad);

	for (i = 0; i < points; i++) {
		long double rad = rad_range*i/points+from_rad-PI/2;
		long double x = cos(rad)*(c->diameter-1)/2 + c->diameter/2;
		long double y = sin(rad)*(c->diameter-1)/2 + c->diameter/2;
		int row = (int)(y*ROW_RATIO);
		int col = (int)x;
		c->rows[row][col] = marker;
	}
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
		long double numerator = ((long)id->x[i]);
		long double denominator = pow((long double)256, (long double)(i+1));
		rad += numerator/denominator;
	}
	return rad*2*PI;
}
