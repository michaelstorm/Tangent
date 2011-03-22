#ifndef CHORD_CIRCLE_H
#define CHORD_CIRCLE_H

#ifndef PI
#define PI 3.1415926535
#endif

#ifndef ROW_RATIO
#define ROW_RATIO 0.5
#endif

struct circle
{
	int diameter;
	int row_count;
	int density;
	char **rows;
};

struct circle *new_circle(int diam, int dens);
void free_circle(struct circle *c);
void draw_circle(struct circle *c, char marker, long double from_rad,
				   long double to_rad);
void print_circle(FILE *fp, struct circle *c);

long double id_to_radians(const chordID *id);

#endif
