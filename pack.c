#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "chord.h"
#include "ctype.h"
#include "dispatcher.h"
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
			return 0;
		}

		bp += pack_single(bp, &p, arg);
	}
	va_end(args);

	return bp - buf;
}

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

uchar msg_buf[BUFSIZE];

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
