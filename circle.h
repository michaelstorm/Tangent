#ifndef CHORD_CIRCLE_H
#define CHORD_CIRCLE_H

#ifndef PI
#define PI ((long double)3.14159265358979323)
#endif

#ifndef ROW_RATIO
#define ROW_RATIO ((long double)0.5)
#endif

struct circle
{
	long double diameter;
	int row_count;
	int density;
	long double rotation;
	char **rows;
};

struct circle *new_circle(long double diam, int dens, long double rot);
void free_circle(struct circle *c);
void draw_circle(struct circle *c, char marker);
long double get_x_coord(struct circle *c, long double rad);
long double get_y(struct circle *c, long double rad);
void draw_arc(struct circle *c, char marker, long double from_rad,
			  long double to_rad);
void draw_radius_automarker(struct circle *c, long double to_rad);
void draw_radius(struct circle *c, char marker, long double to_rad);
void draw_point(struct circle *c, char marker, long double rad);
int measure_text(const char *text, int *width, int *height);
void draw_box(struct circle *c, int x, int y, int width, int height);
void draw_text(struct circle *c, const char *text, int x, int y, int in_box);
void draw_centered_text(struct circle *c, const char *text, int x, int y,
						int in_box);
void print_circle(FILE *fp, struct circle *c);

long double id_to_radians(const chordID *id);

#endif
