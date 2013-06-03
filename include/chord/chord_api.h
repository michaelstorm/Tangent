#ifndef CHORD_API_H
#define CHORD_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef struct in6_addr in6_addr;
typedef struct in_addr in_addr;
#ifdef __APPLE__
typedef u_long ulong;
#endif

enum {
  CHORD_ID_BITS = 160,
  CHORD_ID_LEN = CHORD_ID_BITS/8,
};

typedef struct {
	uchar x[CHORD_ID_LEN];
} chordID;

typedef struct Server Server;

void chord_get_range(Server *srv, chordID *l, chordID *r);
int chord_is_local(Server *srv, chordID *x);
void chord_print_circle(Server *srv, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif
