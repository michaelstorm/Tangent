#ifndef DHASH_PACK_H
#define DHASH_PACK_H

#include "chord.h"

struct DHash;

typedef struct DHashPacketArgs DHashPacketArgs;
struct DHashPacketArgs
{
	ChordPacketArgs chord_args;
	DHash *dhash;
} __attribute__((__packed__));

void dhash_unpack_control_packet(evutil_socket_t sock, short what, void *arg);
int dhash_unpack_query(DHash *dhash, Server *srv, uchar *data, int n,
					   Node *from);
int dhash_unpack_query_reply_success(DHash *dhash, Server *srv, uchar *data,
									  int n, Node *from);
int dhash_unpack_chord_route(Header *header, DHashPacketArgs *args, Data *msg,
							 Node *from);
int dhash_unpack_chord_route_last(Header *header, DHashPacketArgs *args,
								  Data *msg, Node *from);

int dhash_client_unpack_request_reply(int sock, void *ctx,
									  dhash_request_reply_handler handler);
int dhash_pack_control_request_reply(uchar *buf, int code, const char *name,
									 int name_len);
int dhash_pack_query(uchar *buf, in6_addr *addr, ushort port, const char *name,
					 int name_len);
int dhash_pack_query_reply_success(uchar *buf, const char *name, int name_len);
int dhash_pack_query_reply_failure(uchar *buf, const char *name, int name_len);

int dhash_pack_push(uchar *buf, in6_addr *addr, ushort port, const char *name,
					int name_len);
int dhash_unpack_push(DHash *dhash, Server *srv, uchar *data, int n,
					  Node *from);

int dhash_pack_push_reply(uchar *buf, const char *name, int name_len);
int dhash_unpack_push_reply(DHash *dhash, Server *srv, uchar *data, int n,
							Node *from);
int dhash_pack_client_request(uchar *buf, const char *name);

#endif
