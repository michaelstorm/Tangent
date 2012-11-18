#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <ctype.h>
#include "chord/chord.h"
#include "chord/dispatcher.h"
#include "chord/message_print.h"
#include "chord/pack.h"
#include "messages.pb-c.h"

static uchar msg_buf[BUFSIZE];

int pack_header(uchar *buf, int version, int type, const ProtobufCMessage *msg)
{
	Header header = HEADER__INIT;
	header.version = version;
	header.has_version = 1;

	header.type = type;
	header.payload.len = protobuf_c_message_pack(msg, msg_buf);
	header.payload.data = msg_buf;
	LogMessage(TRACE, "Packed header:", &header.base);
	return header__pack(&header, buf);
}

int pack_addr_discover(uchar *buf, uchar *ticket, int ticket_len)
{
	AddrDiscover msg = ADDR_DISCOVER__INIT;
	msg.ticket.len = ticket_len;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;
	LogMessage(TRACE, "Packed message:", &msg.base);
	return pack_chord_header(buf, CHORD_ADDR_DISCOVER, &msg);
}

int pack_addr_discover_reply(uchar *buf, uchar *ticket, int ticket_len,
							 in6_addr *addr)
{
	AddrDiscoverReply msg = ADDR_DISCOVER_REPLY__INIT;
	msg.ticket.len = ticket_len;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;

	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;
	LogMessage(TRACE, "Packed message:", &msg.base);
	return pack_chord_header(buf, CHORD_ADDR_DISCOVER_REPLY, &msg);
}

int pack_data(uchar *buf, int last, uchar ttl, chordID *id, ushort len,
			  const uchar *data)
{
	Data msg = DATA__INIT;
	msg.id.len = CHORD_ID_LEN;
	msg.id.data = id->x;

	msg.ttl = ttl;
	msg.has_ttl = 1;
	msg.last = last;
	msg.has_last = 1;

	msg.data.len = len;
	msg.data.data = (uint8_t *)data;
	LogMessage(TRACE, "Packed message:", &msg.base);
	return pack_chord_header(buf, CHORD_DATA, &msg);
}

int pack_fs(uchar *buf, uchar *ticket, int ticket_len, uchar ttl,
			in6_addr *addr, ushort port)
{
	FindSuccessor msg = FIND_SUCCESSOR__INIT;
	msg.ticket.len = ticket_len;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;

	msg.ttl = ttl;
	msg.has_ttl = 1;

	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;
	msg.port = port;
	LogMessage(TRACE, "Packed message:", &msg.base);
	return pack_chord_header(buf, CHORD_FS, &msg);
}

int pack_fs_reply(uchar *buf, uchar *ticket, int ticket_len, in6_addr *addr,
				  ushort port)
{
	FindSuccessorReply msg = FIND_SUCCESSOR_REPLY__INIT;
	msg.ticket.len = ticket_len;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;

	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;
	msg.port = port;
	LogMessage(TRACE, "Packed message:", &msg.base);
	return pack_chord_header(buf, CHORD_FS_REPLY, &msg);
}

int pack_stab(uchar *buf, in6_addr *addr, ushort port)
{
	Stabilize msg = STABILIZE__INIT;
	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;
	msg.port = port;
	LogMessage(TRACE, "Packed message:", &msg.base);
	return pack_chord_header(buf, CHORD_STAB, &msg);
}

int pack_stab_reply(uchar *buf, in6_addr *addr, ushort port)
{
	StabilizeReply msg = STABILIZE_REPLY__INIT;
	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;
	msg.port = port;
	LogMessage(TRACE, "Packed message:", &msg.base);
	return pack_chord_header(buf, CHORD_STAB_REPLY, &msg);
}

int pack_notify(uchar *buf)
{
	Notify msg = NOTIFY__INIT;
	LogMessage(TRACE, "Packed message:", &msg.base);
	return pack_chord_header(buf, CHORD_NOTIFY, &msg);
}

int pack_ping(uchar *buf, uchar *ticket, int ticket_len, ulong time)
{
	Ping msg = PING__INIT;
	msg.ticket.len = ticket_len;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;
	msg.time = time;
	LogMessage(TRACE, "Packed message:", &msg.base);
	int ret = pack_chord_header(buf, CHORD_PING, &msg);
	return ret;
}

int pack_pong(uchar *buf, uchar *ticket, int ticket_len, ulong time)
{
	Pong msg = PONG__INIT;
	msg.ticket.len = ticket_len;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;
	msg.time = time;
	LogMessage(TRACE, "Packed message:", &msg.base);
	return pack_chord_header(buf, CHORD_PONG, &msg);
}