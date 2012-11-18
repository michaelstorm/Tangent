#ifndef DHASH_PROCESS_H
#define DHASH_PROCESS_H

#include "d_messages.pb-c.h"

int dhash_process_query(Header *header, ChordDataPacketArgs *args, Query *msg,
						Node *from);
int dhash_process_query_reply_success(Header *header, ChordDataPacketArgs *args,
									  QueryReplySuccess *msg, Node *from);
int dhash_process_query_reply_failure(Header *header, ChordDataPacketArgs *args,
									  QueryReplyFailure *msg, Node *from);
int dhash_process_push(Header *header, ChordDataPacketArgs *args, Push *msg,
					   Node *from);
int dhash_process_push_reply(Header *header, ChordDataPacketArgs *args,
							 PushReply *msg, Node *from);
int dhash_process_client_request(Header *header, ControlPacketArgs *args,
								 ClientRequest *msg, Node *from);

#endif
