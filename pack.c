#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "chord.h"
#include "ctype.h"
#include "messages.pb-c.h"

static int sizeof_packed_single(char **fmt)
{
	int len;
	switch (**fmt) {
	case 'c': (*fmt)++; return sizeof(uint8_t);
	case 's': (*fmt)++; return sizeof(uint16_t);
	case 'l': (*fmt)++; return sizeof(uint32_t);
	case 'x': (*fmt)++; return CHORD_ID_LEN;
	case 't': (*fmt)++; return TICKET_LEN;
	case '6': (*fmt)++; return 16;
	case '0': case '*': (*fmt)++; return sizeof_packed_single(fmt);
	case '<':
		if (isdigit(*((*fmt)+1))) {
			sscanf(*fmt, "<%d>", &len);
			*fmt = strchr(*fmt, '>')+1;
		}
		else {
			(*fmt)++;
			len = sizeof_packed_single(fmt);
			(*fmt)++;
		}
		return len;
	}

	assert(0);
	return -1;
}

static int sizeof_unpacked_single(char **fmt)
{
	int len;
	switch (**fmt) {
	case 'c': (*fmt)++; return sizeof(uint8_t);
	case 's': (*fmt)++; return sizeof(uint16_t);
	case 'l': (*fmt)++; return sizeof(uint32_t);
	case 'x': (*fmt)++; return sizeof(chordID);
	case 't': (*fmt)++; return TICKET_LEN;
	case '6': (*fmt)++; return sizeof(in6_addr);
	case '0': case '*': (*fmt)++; return sizeof_unpacked_single(fmt);
	case '<':
		if (isdigit(*((*fmt)+1))) {
			sscanf(*fmt, "<%d>", &len);
			*fmt = strchr(*fmt, '>')+1;
		}
		else {
			(*fmt)++;
			len = sizeof_unpacked_single(fmt);
			(*fmt)++;
		}
		return len;
	}

	assert(0);
	return -1;
}

static int pack_single(uchar *bp, char **fmt, void *arg)
{
	chordID *id;
	ushort s;
	ulong l;
	uchar *t;
	in6_addr *v6addr;
	int len;

	switch (**fmt) {
	case 'c':	 /* char */
		*bp = *(uchar *)arg;
		(*fmt)++;
		return sizeof(uchar);
	case 's':	 /* short */
		s = *(ushort *)arg;
		s = htons(s);
		memmove(bp, (char *)&s, sizeof(uint16_t));
		(*fmt)++;
		return sizeof(ushort);
	case 'l':	 /* long */
		l = *(ulong *)arg;
		l = htonl(l);
		memmove(bp, (char *)&l, sizeof(uint32_t));
		(*fmt)++;
		return sizeof(ulong);
	case 'x':	 /* id */
		id = (chordID *)arg;
		memmove(bp, id->x, CHORD_ID_LEN);
		(*fmt)++;
		return CHORD_ID_LEN;
	case 't':	 /* ticket */
		t = (uchar *)arg;
		memmove(bp, t, TICKET_LEN);
		(*fmt)++;
		return TICKET_LEN;
	case '6':
		v6addr = (in6_addr *)arg;
		memmove(bp, v6addr->s6_addr, 16);
		(*fmt)++;
		return 16;
	case '<':
		t = (uchar *)arg;
		sscanf(*fmt, "<%d>", &len);
		memmove(bp, t, len);
		*fmt = strchr(*fmt, '>')+1;
		return len;
	case '*':
		(*fmt)++;
		len = sizeof_packed_single(fmt);
		return len;
	case '0':
		(*fmt)++;
		len = sizeof_packed_single(fmt);
		bzero(bp, len);
		return len;
	}

	return -1;
}

/* pack: pack binary items into buf, return length */
int pack(uchar *buf, const char *fmt, ...)
{
	//uchar *orig_buf = buf;
	//buf += 2048;

	va_list args;
	char *p;
	void *arg;

	uchar *bp = buf;
	va_start(args, fmt);
	for (p = (char *)fmt; *p != '\0';) {
		switch (*p) {
		case 'c': case 's': case 'l':
		{
			ulong l = va_arg(args, uint32_t);
			arg = &l;
			break;
		}

		case 'x': case 't': case '6':
			arg = va_arg(args, void *);
			break;

		case '*': case '0':
			if (*(p+1) == '<' && !isdigit(*(p+2))) {
				ulong l = va_arg(args, uint32_t); // length is the next argument
				arg = &l;
				p += 2; // skip the * or 0 and the opening bracket
				bp += pack_single(bp, &p, arg);
				p++; // skip the closing bracket

				if (*(p-4) == '0')
					bzero(bp, l);
				bp += l;
				continue;
			}
			else {
				arg = NULL;
				break;
			}

		case '<':
		{
			// if length is specified in the format, just write the data
			if (isdigit(*(p+1))) {
				arg = va_arg(args, void *);
				break;
			}
			else {
				// otherwise, length is the next argument
				ulong l = va_arg(args, uint32_t);
				arg = &l;
				p++; // skip the opening bracket
				bp += pack_single(bp, &p, arg);
				p++; // skip the closing bracket

				// write the data
				uchar *data = va_arg(args, uchar *);
				memcpy(bp, data, l);
				bp += l;
				continue;
			}
		}

		default:
			va_end(args);
			return CHORD_PACK_ERROR;
		}

		bp += pack_single(bp, &p, arg);
	}
	va_end(args);

	return bp - buf;
}

/**********************************************************************/

static void unpack_single(uchar **bp, uchar **op, char **fmt)
{
	int len;
	switch (**fmt) {
	case 'c':	 /* char */
		**op = **bp;
		(*op)++;
		(*bp)++;
		(*fmt)++;
		break;
	case 's':	 /* short */
		**(ushort **)op = ntohs(**(ushort **)bp);
		*bp += sizeof(uint16_t);
		*op += sizeof(uint16_t);
		(*fmt)++;
		break;
	case 'l':	 /* long */
		**(ulong **)op = ntohl(**(ulong **)bp);
		*bp += sizeof(uint32_t);
		*op += sizeof(uint32_t);
		(*fmt)++;
		break;
	case 'x':	 /* id */
		memmove((*(chordID **)op)->x, *bp, CHORD_ID_LEN);
		*bp += CHORD_ID_LEN;
		*op += sizeof(chordID);
		(*fmt)++;
		break;
	case 't':	 /* ticket */
		memmove(*op, *bp, TICKET_LEN);
		*bp += TICKET_LEN;
		*op += TICKET_LEN;
		(*fmt)++;
		break;
	case '6':
		memmove((*(in6_addr **)op)->s6_addr, *bp, 16);
		*bp += 16;
		*op += sizeof(in6_addr);
		(*fmt)++;
		break;
	case '<':
		sscanf(*fmt, "<%d>", &len);
		memmove(*op, *bp, len);
		*bp += len;
		*op += len;
		*fmt = strchr(*fmt, '>')+1;
		break;
	case '0':    /* skip */
		(*fmt)++;
		len = sizeof_unpacked_single(fmt);
		bzero(*op, len);
		*bp += len;
		break;
	case '*':
		(*fmt)++;
		*bp += sizeof_unpacked_single(fmt);
		break;
	}
}

/* unpack: unpack binary items from buf, return length */
int unpack(uchar *buf, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	char *p = (char *)fmt;
	void *arg;

	uchar *bp = buf;
	while (*p != '\0') {
		switch (*p) {
		case 'c': case 's': case 'l':
			arg = va_arg(args, uint32_t *);
			break;

		case 'x': case 't': case '6':
			arg = va_arg(args, void *);
			break;

		case '*': case '0':
			if (*p == '0')
				arg = va_arg(args, void *);
			else
				arg = NULL;
			break;

		case '<':
			if (isdigit(*(p+1))) {
				arg = va_arg(args, void *);
				break;
			}
			else {
				p++;
				uchar *bp_copy = bp;
				char *p_copy = p;

				ulong len = 0;
				ulong *len_p = &len;
				unpack_single(&bp_copy, (uchar **)&len_p, &p_copy);

				arg = va_arg(args, void *);
				unpack_single(&bp, (uchar **)&arg, &p);
				p++;

				arg = va_arg(args, void *);
				memmove(arg, bp, len);
				bp += len;
				continue;
			}
		}

		unpack_single(&bp, (uchar **)&arg, &p);
	}
	va_end(args);
	return bp - buf;
}

/**********************************************************************/

int sizeof_packed_fmt(const char *fmt)
{
	int len = 0;
	char *p = (char *)fmt;
	while (*p != '\0')
		len += sizeof_packed_single(&p);
	return len;
}

int sizeof_unpacked_fmt(const char *fmt)
{
	int len = 0;
	char *p = (char *)fmt;
	while (*p != '\0')
		len += sizeof_unpacked_single(&p);
	return len;
}

/**********************************************************************/

typedef void *(*unpack_fn)(ProtobufCAllocator *, size_t, const uint8_t *);
typedef int (*process_fn)(Server *, void *, Node *);

struct packet_handler
{
	unpack_fn unpack;
	process_fn process;
};

static struct packet_handler handlers[] = {
	{(unpack_fn)addr_discover__unpack, (process_fn)process_addr_discover},
	{(unpack_fn)addr_discover_reply__unpack,
									(process_fn)process_addr_discover_reply},
	{(unpack_fn)data__unpack, (process_fn)process_route},
	{(unpack_fn)data__unpack, (process_fn)process_route_last},
	{(unpack_fn)find_successor__unpack, (process_fn)process_fs},
	{(unpack_fn)find_successor_reply__unpack, (process_fn)process_fs_reply},
	{(unpack_fn)stabilize__unpack, (process_fn)process_stab},
	{(unpack_fn)stabilize_reply__unpack, (process_fn)process_stab_reply},
	{(unpack_fn)notify__unpack, (process_fn)process_notify},
	{(unpack_fn)ping__unpack, (process_fn)process_ping},
	{(unpack_fn)pong__unpack, (process_fn)process_pong},
};

/* dispatch: unpack and process packet */
int dispatch(Server *srv, int n, uchar *buf, Node *from)
{
	uchar type;
	int res;

	type = buf[0];
	if (type > CHORD_ADDR_DISCOVER_REPL
		&& IN6_IS_ADDR_UNSPECIFIED(&srv->node.addr))
		res = CHORD_ADDR_UNDISCOVERED;
	else if (type < NELEMS(handlers)) {
		void *msg = handlers[type].unpack(NULL, n-1, buf+1);

		if (srv->packet_handlers[type]
			&& (res = srv->packet_handlers[type](srv->packet_handler_ctx, srv,
												 msg, from)))
			return res;

		if (msg == NULL) {
			fprintf(stderr, "error unpacking packet\n");
			return 0;
		}

		res = handlers[type].process(srv, msg, from);
	}
	else {
		weprintf("bad packet type 0x%02x", type);
		res = CHORD_PROTOCOL_ERROR;
	}

	if (res < 0) {
		char *err_str;
		switch (res) {
		case CHORD_PROTOCOL_ERROR:
			err_str = "protocol error";
			break;
		case CHORD_TTL_EXPIRED:
			err_str = "time-to-live expired";
			break;
		case CHORD_INVALID_TICKET:
			err_str = "invalid ticket";
			break;
		case CHORD_PACK_ERROR:
			err_str = "internal packing error";
			break;
		case CHORD_ADDR_UNDISCOVERED:
			err_str = "received routing packet before address discovery";
			break;
		default:
			err_str = "unknown error";
			break;
		}

		weprintf("dropping packet [type 0x%02x size %d] from %s:%hu (%s)", type,
				 n, v6addr_to_str(&from->addr), from->port, err_str);
	}

	return res;
}

int pack_addr_discover(uchar *buf, uchar *ticket)
{
	AddrDiscover msg = ADDR_DISCOVER__INIT;
	msg.ticket.len = TICKET_LEN;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;
	addr_discover__pack(&msg, buf+1);

	buf[0] = CHORD_ADDR_DISCOVER;
	return addr_discover__get_packed_size(&msg)+1;
}

int pack_addr_discover_reply(uchar *buf, uchar *ticket, in6_addr *addr)
{
	AddrDiscoverReply msg = ADDR_DISCOVER_REPLY__INIT;
	msg.ticket.len = TICKET_LEN;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;

	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;

	addr_discover_reply__pack(&msg, buf+1);
	buf[0] = CHORD_ADDR_DISCOVER_REPL;
	return addr_discover_reply__get_packed_size(&msg)+1;
}

/**********************************************************************/

/* pack_data: pack data packet */
int pack_data(uchar *buf, uchar type, uchar ttl, chordID *id, ushort len,
			  const uchar *data)
{
	Data msg = DATA__INIT;
	msg.id.len = CHORD_ID_LEN;
	msg.id.data = id->x;

	msg.ttl = ttl;

	msg.data.len = len;
	msg.data.data = (uint8_t *)data;

	data__pack(&msg, buf+1);
	buf[0] = type;
	return data__get_packed_size(&msg)+1;
}

/**********************************************************************/

/* pack_fs: pack find_successor packet */
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
	find_successor__pack(&msg, buf+1);
	buf[0] = CHORD_FS;
	return find_successor__get_packed_size(&msg)+1;
}

/**********************************************************************/

/* pack_fs_repl: pack find_successor reply packet */
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
	find_successor_reply__pack(&msg, buf+1);
	buf[0] = CHORD_FS_REPL;
	return find_successor_reply__get_packed_size(&msg)+1;
}

/**********************************************************************/

/* pack_stab: pack stabilize packet */
int pack_stab(uchar *buf, in6_addr *addr, ushort port)
{
	Stabilize msg = STABILIZE__INIT;
	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;

	msg.port = port;
	stabilize__pack(&msg, buf+1);
	buf[0] = CHORD_STAB;
	return stabilize__get_packed_size(&msg)+1;
}

/**********************************************************************/

/* pack_stab_repl: pack stabilize reply packet */
int pack_stab_reply(uchar *buf, in6_addr *addr, ushort port)
{
	StabilizeReply msg = STABILIZE_REPLY__INIT;
	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;

	msg.port = port;
	stabilize_reply__pack(&msg, buf+1);
	buf[0] = CHORD_STAB_REPL;
	return stabilize_reply__get_packed_size(&msg)+1;
}

/**********************************************************************/

/* pack_notify: pack notify packet */
int pack_notify(uchar *buf)
{
	Notify msg = NOTIFY__INIT;
	notify__pack(&msg, buf+1);
	buf[0] = CHORD_NOTIFY;
	return notify__get_packed_size(&msg)+1;
}

/**********************************************************************/

/* pack_ping: pack ping packet */
int pack_ping(uchar *buf, uchar *ticket, ulong time)
{
	Ping msg = PING__INIT;
	msg.ticket.len = TICKET_LEN;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;

	msg.time = time;
	ping__pack(&msg, buf+1);
	buf[0] = CHORD_PING;
	return ping__get_packed_size(&msg)+1;
}

/**********************************************************************/

/* pack_pong: pack pong packet */
int pack_pong(uchar *buf, uchar *ticket, ulong time)
{
	Pong msg = PONG__INIT;
	msg.ticket.len = TICKET_LEN;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;

	msg.time = time;
	pong__pack(&msg, buf+1);
	buf[0] = CHORD_PONG;
	return pong__get_packed_size(&msg)+1;
}
