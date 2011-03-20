#ifndef DHASH_PACK_H
#define DHASH_PACK_H

struct DHash;
struct Server;

void dhash_unpack_control_packet(evutil_socket_t sock, short what, void *arg);
int dhash_unpack_chord_packet(struct DHash *dhash, struct Server *srv, int n,
							  uchar *buf, struct Node *from);

#endif
