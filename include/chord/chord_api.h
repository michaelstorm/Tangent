#ifndef CHORD_API_H
#define CHORD_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#ifdef __APPLE__
typedef u_long ulong;
#endif

enum {
  CHORD_ID_BITS = 160,
  CHORD_ID_LEN = CHORD_ID_BITS/8,
};

typedef struct {
	unsigned char x[CHORD_ID_LEN];
} chordID;

struct ChordServer;

void chord_get_range(struct ChordServer *srv, chordID *l, chordID *r);
int chord_is_local(struct ChordServer *srv, chordID *x);
void chord_print_circle(struct ChordServer *srv, FILE *fp);

chordID chord_id_successor(chordID id, int n);
chordID chord_id_predecessor(chordID id, int n);

#ifdef __cplusplus
}
#endif

#endif
