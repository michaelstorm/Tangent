#ifndef CHORD_CIRCLE_H
#define CHORD_CIRCLE_H

#ifndef PI
#define PI 3.141592653589793238462643383279
#endif

#ifndef ROW_RATIO
#define ROW_RATIO 0.5
#endif

struct circle
{
	int diameter;
	int row_count;
	int density;
	long double rotation;
	char **rows;
};

struct circle *new_circle(int diam, int dens, long double rot);
void free_circle(struct circle *c);
void draw_circle(struct circle *c, char marker);
void draw_arc(struct circle *c, char marker, long double from_rad,
			  long double to_rad);
void draw_radius_automarker(struct circle *c, long double to_rad);
void draw_radius(struct circle *c, char marker, long double to_rad);
void draw_point(struct circle *c, char marker, long double rad);
void print_circle(FILE *fp, struct circle *c);

long double id_to_radians(const chordID *id);

#endif
