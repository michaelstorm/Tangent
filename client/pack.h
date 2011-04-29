#ifndef DHASH_PACK_H
#define DHASH_PACK_H

#include "chord.h"

#define DHASH_WIRE_VERSION 1

struct DHash;

#define pack_dhash_header(buf, type, msg) \
	pack_header(buf, DHASH_WIRE_VERSION, type, (const ProtobufCMessage *)msg)

void dhash_unpack_control_packet(evutil_socket_t sock, short what, void *arg);
int dhash_unpack_query(DHash *dhash, Server *srv, uchar *data, int n,
					   Node *from);
int dhash_unpack_query_reply_success(DHash *dhash, Server *srv, uchar *data,
									  int n, Node *from);
int dhash_unpack_chord_data(Header *header, DHashPacketArgs *args, Data *msg,
							Node *from);

int dhash_client_unpack_request_reply(uchar *buf, int n, void *ctx,
									  dhash_request_reply_handler handler);
int dhash_pack_control_request_reply(uchar *buf, int code, const uchar *name,
									 int name_len);
int dhash_pack_query(uchar *buf, in6_addr *addr, ushort port, const uchar *name,
					 int name_len);
int dhash_pack_query_reply_success(uchar *buf, const uchar *name, int name_len);
int dhash_pack_query_reply_failure(uchar *buf, const uchar *name, int name_len);

int dhash_pack_push(uchar *buf, in6_addr *addr, ushort port, const uchar *name,
					int name_len);
int dhash_unpack_push(DHash *dhash, Server *srv, uchar *data, int n,
					  Node *from);

int dhash_pack_push_reply(uchar *buf, const uchar *name, int name_len);
int dhash_unpack_push_reply(DHash *dhash, Server *srv, uchar *data, int n,
							Node *from);
int dhash_pack_client_request(uchar *buf, const uchar *name, int name_len);

#endif
