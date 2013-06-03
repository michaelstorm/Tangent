#ifndef CHORD_API_H
#define CHORD_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "chord/visibility.h"

enum {
  CHORD_ID_BITS = 160,
  CHORD_ID_BYTES = CHORD_ID_BITS/8,
};

typedef struct {
	unsigned char x[CHORD_ID_BYTES];
} chordID;

struct ChordServer;

void chord_get_range(struct ChordServer *srv, chordID *l, chordID *r) DLL_PUBLIC;
void chord_print_circle(struct ChordServer *srv, FILE *fp) DLL_PUBLIC;

int chord_id_is_local(struct ChordServer *srv, chordID *x) DLL_PUBLIC;
chordID chord_id_successor(chordID id, int n) DLL_PUBLIC;
chordID chord_id_predecessor(chordID id, int n) DLL_PUBLIC;

#ifdef __cplusplus
}
#endif

#endif
