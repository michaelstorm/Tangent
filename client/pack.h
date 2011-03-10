#ifndef DHASH_PACK_H
#define DHASH_PACK_H

int dhash_unpack_control_packet(DHash *dhash, int sock);
int dhash_unpack_chord_packet(struct DHash *dhash, struct Server *srv, int n,
							  uchar *buf, struct Node *from);

#endif
