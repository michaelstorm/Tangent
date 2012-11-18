#ifndef PACK_H
#define PACK_H

#include "chord/chord_api.h"
#include "messages.pb-c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define pack_chord_header(buf, type, msg) \
	pack_header(buf, CHORD_WIRE_VERSION, type, (const ProtobufCMessage *)msg)

int pack_header(uchar *buf, int version, int type, const ProtobufCMessage *msg);
int pack_addr_discover(uchar *buf, uchar *ticket, int ticket_len);
int pack_addr_discover_reply(uchar *buf, uchar *ticket, int ticket_len,
							 in6_addr *addr);
int pack_data(uchar *buf, int last, uchar ttl, chordID *id, ushort len,
			  const uchar *data);
int pack_fs(uchar *buf, uchar *ticket, int ticket_len, uchar ttl,
			in6_addr *addr, ushort port);
int pack_fs_reply(uchar *buf, uchar *ticket, int ticket_len, in6_addr *addr,
				  ushort port);
int pack_stab(uchar *buf, in6_addr *addr, ushort port);
int pack_stab_reply(uchar *buf, in6_addr *addr, ushort port);
int pack_notify(uchar *buf);
int pack_ping(uchar *buf, uchar *ticket, int ticket_len, ulong time);
int pack_pong(uchar *buf, uchar *ticket, int ticket_len, ulong time);

#ifdef __cplusplus
}
#endif

#endif