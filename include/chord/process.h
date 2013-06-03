#ifndef PROCESS_H
#define PROCESS_H

#include "messages.pb-c.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ChordPacketArgs
{
	ChordServer *srv;
} __attribute__((__packed__));
typedef struct ChordPacketArgs ChordPacketArgs;

Node *next_route_node(ChordServer *srv, chordID *id, int last, int *next_is_last);
int process_addr_discover(Header *header, ChordPacketArgs *args,
						  AddrDiscover *msg, Node *from);
int process_addr_discover_reply(Header *header, ChordPacketArgs *args,
								AddrDiscoverReply *msg, Node *from);
int process_data(Header *header, ChordPacketArgs *args, Data *msg, Node *from);
int process_fs(Header *header, ChordPacketArgs *args, FindSuccessor *msg,
			   Node *from);
int process_fs_reply(Header *header, ChordPacketArgs *args,
					 FindSuccessorReply *msg, Node *from);
int process_stab(Header *header, ChordPacketArgs *args, Stabilize *msg,
				 Node *from);
int process_stab_reply(Header *header, ChordPacketArgs *args,
					   StabilizeReply *msg, Node *from);
int process_notify(Header *header, ChordPacketArgs *args, Notify *msg,
				   Node *from);
int process_ping(Header *header, ChordPacketArgs *args, Ping *msg, Node *from);
int process_pong(Header *header, ChordPacketArgs *args, Pong *msg, Node *from);
void process_error(Header *header, ChordPacketArgs *args, void *msg, Node *from,
				   int error);

#ifdef __cplusplus
}
#endif

#endif
