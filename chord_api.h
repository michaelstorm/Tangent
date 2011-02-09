#ifndef CHORD_API_H
#define CHORD_API_H

typedef unsigned char byte;

enum {
  CHORD_ID_BITS = 160,
  CHORD_ID_LEN = CHORD_ID_BITS/8,
};

typedef struct {
	byte x[CHORD_ID_LEN];
} chordID;

/* init: initialize chord server, provide configuration file */
int chord_init(char *conf_file);

/* route: forward message M towards the root of key K. */
void chord_route(chordID *k, char *data, int len);

void chord_cleanup(int signum);

#endif
