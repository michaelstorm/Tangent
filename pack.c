#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "chord.h"
#include "ctype.h"
#include "dispatcher.h"
#include "messages.pb-c.h"

static uchar msg_buf[BUFSIZE];

int pack_header(uchar *buf, int type, uchar *payload, int n)
{
	Header header = HEADER__INIT;
	header.type = type;
	header.payload.len = n;
	header.payload.data = payload;
	return header__pack(&header, buf);
}

int pack_addr_discover(uchar *buf, uchar *ticket)
{
	AddrDiscover msg = ADDR_DISCOVER__INIT;
	msg.ticket.len = TICKET_LEN;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;
	int n = addr_discover__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_ADDR_DISCOVER, msg_buf, n);
}

int pack_addr_discover_reply(uchar *buf, uchar *ticket, in6_addr *addr)
{
	AddrDiscoverReply msg = ADDR_DISCOVER_REPLY__INIT;
	msg.ticket.len = TICKET_LEN;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;

	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;

	int n = addr_discover_reply__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_ADDR_DISCOVER_REPL, msg_buf, n);
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

	int n = data__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_DATA, msg_buf, n);
}

int pack_fs(uchar *buf, uchar *ticket, uchar ttl, in6_addr *addr, ushort port)
{
	FindSuccessor msg = FIND_SUCCESSOR__INIT;
	msg.ticket.len = TICKET_LEN;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;

	msg.ttl = ttl;
	msg.has_ttl = 1;

	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;

	msg.port = port;
	int n = find_successor__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_FS, msg_buf, n);
}

int pack_fs_reply(uchar *buf, uchar *ticket, in6_addr *addr,
				 ushort port)
{
	FindSuccessorReply msg = FIND_SUCCESSOR_REPLY__INIT;
	msg.ticket.len = TICKET_LEN;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;

	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;

	msg.port = port;
	int n = find_successor_reply__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_FS_REPL, msg_buf, n);
}

int pack_stab(uchar *buf, in6_addr *addr, ushort port)
{
	Stabilize msg = STABILIZE__INIT;
	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;

	msg.port = port;
	int n = stabilize__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_STAB, msg_buf, n);
}

int pack_stab_reply(uchar *buf, in6_addr *addr, ushort port)
{
	StabilizeReply msg = STABILIZE_REPLY__INIT;
	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;

	msg.port = port;
	int n = stabilize_reply__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_STAB_REPL, msg_buf, n);
}

int pack_notify(uchar *buf)
{
	Notify msg = NOTIFY__INIT;
	int n = notify__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_NOTIFY, msg_buf, n);
}

int pack_ping(uchar *buf, uchar *ticket, ulong time)
{
	Ping msg = PING__INIT;
	msg.ticket.len = TICKET_LEN;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;

	msg.time = time;
	int n = ping__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_PING, msg_buf, n);
}

int pack_pong(uchar *buf, uchar *ticket, ulong time)
{
	Pong msg = PONG__INIT;
	msg.ticket.len = TICKET_LEN;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;

	msg.time = time;
	int n = pong__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_PONG, msg_buf, n);
}
