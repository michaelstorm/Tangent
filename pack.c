#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "chord.h"

/* pack: pack binary items into buf, return length */

int pack(uchar *buf, const char *fmt, ...)
{
	va_list args;
	const char *p;
	uchar *bp;
	chordID *id;
	ushort s;
	ulong l;
	uchar *t;
	in6_addr *v6addr;
	int zero_len;

	bp = buf;
	va_start(args, fmt);
	for (p = fmt; *p != '\0'; p++) {
		switch (*p) {
		case 'c':	 /* char */
			*bp++ = va_arg(args, int);
			break;
		case 's':	 /* short */
			s = va_arg(args, int);
			s = htons(s);
			memmove(bp, (char *)&s, sizeof(uint16_t));
			bp += sizeof(ushort);
			break;
		case 'l':	 /* long */
			l = va_arg(args, ulong);
			l = htonl(l);
			memmove(bp, (char *)&l, sizeof(uint32_t));
			bp += sizeof(ulong);
			break;
		case 'x':	 /* id */
			id = va_arg(args, chordID *);
			memmove(bp, id->x, CHORD_ID_LEN);
			bp += CHORD_ID_LEN;
			break;
		case 't':	 /* ticket */
			t = va_arg(args, uchar *);
			memmove(bp, t, TICKET_LEN);
			bp += TICKET_LEN;
			break;
		case '6':
			v6addr = va_arg(args, in6_addr *);
			memmove(bp, v6addr->s6_addr, 16);
			bp += 16;
			break;
		case '0':    /* zero/skip */
		case '*':
			switch (*(++p)) {
			case 'c': zero_len = sizeof(uint8_t); break;
			case 's': zero_len = sizeof(uint16_t); break;
			case 'l': zero_len = sizeof(uint32_t); break;
			case 'x': zero_len = CHORD_ID_LEN; break;
			case 't': zero_len = TICKET_LEN; break;
			case '6': zero_len = 16; break;
			default: va_end(args); return CHORD_PACK_ERROR;
			}
			if (*(p-1) == '0')
				bzero(bp, zero_len);
			bp += zero_len;
			break;
		default:	 /* illegal type character */
			va_end(args);
			return CHORD_PACK_ERROR;
		}
	}
	va_end(args);
	return bp - buf;
}

/**********************************************************************/

/* unpack: unpack binary items from buf, return length */
int unpack(uchar *buf, const char *fmt, ...)
{
	va_list args;
	const char *p;
	uchar *bp, *pc;
	chordID *id;
	ushort *ps;
	ulong *pl;
	in6_addr *v6addr;
	int zero_len;

	bp = buf;
	va_start(args, fmt);
	for (p = fmt; *p != '\0'; p++) {
		switch (*p) {
		case 'c':	 /* char */
			pc = va_arg(args, uchar*);
			*pc = *bp++;
			break;
		case 's':	 /* short */
			ps = va_arg(args, ushort*);
			*ps = ntohs(*(uint16_t*)bp);
			bp += sizeof(uint16_t);
			break;
		case 'l':	 /* long */
			pl = va_arg(args, ulong*);
			*pl	= ntohl(*(uint32_t*)bp);
			bp += sizeof(uint32_t);
			break;
		case 'x':	 /* id */
			id = va_arg(args, chordID *);
			memmove(id->x, bp, CHORD_ID_LEN);
			bp += CHORD_ID_LEN;
			break;
		case 't':	 /* ticket */
			pc = va_arg(args, uchar *);
			memmove(pc, bp, TICKET_LEN);
			bp += TICKET_LEN;
			break;
		case '6':
			v6addr = va_arg(args, in6_addr *);
			memmove(v6addr->s6_addr, bp, 16);
			bp += 16;
			break;
		case '0':    /* skip */
		case '*':
			switch (*(++p)) {
			case 'c': zero_len = sizeof(uint8_t); break;
			case 's': zero_len = sizeof(uint16_t); break;
			case 'l': zero_len = sizeof(uint32_t); break;
			case 'x': zero_len = CHORD_ID_LEN; break;
			case 't': zero_len = TICKET_LEN; break;
			case '6': zero_len = 16; break;
			default: va_end(args); return CHORD_PACK_ERROR;
			}
			bp += zero_len;
			break;
		default:	 /* illegal type character */
			va_end(args);
			return CHORD_PACK_ERROR;
		}
	}
	va_end(args);
	return bp - buf;
}

/**********************************************************************/

int sizeof_fmt(const char *fmt)
{
	int len = 0;
	int i;

	for (i = 0; i < strlen(fmt); i++) {
		switch (fmt[i]) {
		case 'c':	 /* char */
			len += sizeof(char);
			break;
		case 's':	 /* short */
			len += sizeof(ushort);
			break;
		case 'l':	 /* long */
			len += sizeof(ulong);
			break;
		case 'x':	 /* id */
			len += CHORD_ID_LEN;
			break;
		case 't':	 /* ticket */
			len += TICKET_LEN;
			break;
		case '6':
			len += 16;
			break;
		case '0':	 /* zero/skip */
		case '*':
			switch (fmt[i+1]) {
			case 'c': len = sizeof(uint8_t); break;
			case 's': len = sizeof(uint16_t); break;
			case 'l': len = sizeof(uint32_t); break;
			case 'x': len += CHORD_ID_LEN; break;
			case 't': len += TICKET_LEN; break;
			default:
				eprintf("fmt_len: illegal type character.\n");
				return CHORD_PACK_ERROR;
			}
		default:
			eprintf("fmt_len: illegal type character.\n");
			return CHORD_PACK_ERROR;
		}
	}
	return len;
}

/**********************************************************************/

static int (*unpackfn[])(Server *, int, uchar *, Node *) = {
	unpack_addr_discover,
	unpack_addr_discover_repl,
	unpack_data,
	unpack_data,
	unpack_fs,
	unpack_fs_repl,
	unpack_stab,
	unpack_stab_repl,
	unpack_notify,
	unpack_ping,
	unpack_pong,
	unpack_fingers_get,
	unpack_fingers_repl,
	unpack_traceroute,
	unpack_traceroute,
	unpack_traceroute_repl,
};

/* dispatch: unpack and process packet */
int dispatch(Server *srv, int n, uchar *buf, Node *from)
{
	uchar type;
	int res;

	type = buf[0];

	if (type < NELEMS(unpackfn)) {
		if (srv->packet_handlers[type]
			&& (res = srv->packet_handlers[type](srv->packet_handler_ctx,
														srv, n, buf, from)))
			return res;

		if (type > CHORD_ADDR_DISCOVER_REPL
			&& IN6_IS_ADDR_UNSPECIFIED(&srv->node.addr))
			res = CHORD_ADDR_UNDISCOVERED;
		else
			res = (*unpackfn[type])(srv, n, buf, from);
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
	return pack(buf, "ct", CHORD_ADDR_DISCOVER, ticket);
}

int unpack_addr_discover(Server *srv, int n, uchar *buf, Node *from)
{
	byte type;
	uchar ticket[TICKET_LEN];

	if (unpack(buf, "ct", &type, ticket) != n) {
		fprintf(stderr, "expected %d but got %d\n", n, unpack(buf, "ct", &type, ticket));
		return CHORD_PROTOCOL_ERROR;
	}

	assert(type == CHORD_ADDR_DISCOVER);
	return process_addr_discover(srv, ticket, from);
}

int pack_addr_discover_repl(uchar *buf, uchar *ticket, in6_addr *addr)
{
	return pack(buf, "ct6", CHORD_ADDR_DISCOVER_REPL, ticket, addr);
}

int unpack_addr_discover_repl(Server *srv, int n, uchar *buf, Node *from)
{
	byte type;
	uchar ticket[TICKET_LEN];
	in6_addr addr;

	if (unpack(buf, "ct6", &type, ticket, &addr) != n)
		return CHORD_PROTOCOL_ERROR;

	assert(type == CHORD_ADDR_DISCOVER_REPL);
	return process_addr_discover_repl(srv, ticket, &addr, from);
}

/**********************************************************************/

/* pack_data: pack data packet */
int pack_data(uchar *buf, uchar type, byte ttl, chordID *id, ushort len,
			  uchar *data)
{
	int n;

	n = pack(buf, "ccxs", type, ttl, id, len);
	if (n >= 0) {
		memmove(buf + n, data, len);
		n += len;
	}
	return n;
}

/**********************************************************************/

/* unpack_data: unpack and process data packet */
int unpack_data(Server *srv, int n, uchar *buf, Node *from)
{
	uchar type;
	byte ttl;
	int len;
	chordID id;
	ushort pkt_len;

	len = unpack(buf, "ccxs", &type, &ttl, &id, &pkt_len);
	if (len < 0 || len + pkt_len != n)
		return CHORD_PROTOCOL_ERROR;
	assert(type == CHORD_ROUTE || type == CHORD_ROUTE_LAST);
	return process_data(srv, type, ttl, &id, pkt_len, buf + len, from);
}

/**********************************************************************/

/* pack_fs: pack find_successor packet */
int pack_fs(uchar *buf, uchar *ticket, byte ttl, in6_addr *addr, ushort port)
{
	return pack(buf, "ctc6s", CHORD_FS, ticket, ttl, addr, port);
}

/**********************************************************************/

/* unpack_fs: unpack and process find_successor packet */
int unpack_fs(Server *srv, int n, uchar *buf, Node *from)
{
	uchar type;
	uchar ticket[TICKET_LEN];
	byte ttl;
	in6_addr addr;
	ushort port;

	if (unpack(buf, "ctc6s", &type, ticket, &ttl, &addr, &port) != n)
		return CHORD_PROTOCOL_ERROR;
	assert(type == CHORD_FS);
	return process_fs(srv, ticket, ttl, &addr, port);
}

/**********************************************************************/

/* pack_fs_repl: pack find_successor reply packet */
int pack_fs_repl(uchar *buf, uchar *ticket, in6_addr *addr,
				 ushort port)
{
	return pack(buf, "ct6s", CHORD_FS_REPL, ticket, addr, port);
}

/**********************************************************************/

/* unpack_fs_repl: unpack and process find_successor reply packet */
int unpack_fs_repl(Server *srv, int n, uchar *buf, Node *from)
{
	uchar type;
	in6_addr addr;
	ushort port;
	uchar ticket[TICKET_LEN];

	if (unpack(buf, "ct6s", &type, ticket, &addr, &port) != n)
		return CHORD_PROTOCOL_ERROR;
	assert(type == CHORD_FS_REPL);
	return process_fs_repl(srv, ticket, &addr, port);
}

/**********************************************************************/

/* pack_stab: pack stabilize packet */
int pack_stab(uchar *buf, in6_addr *addr, ushort port)
{
	return pack(buf, "c6s", CHORD_STAB, addr, port);
}

/**********************************************************************/

/* unpack_stab: unpack and process stabilize packet */
int unpack_stab(Server *srv, int n, uchar *buf, Node *from)
{
	uchar type;
	in6_addr addr;
	ushort port;

	if (unpack(buf, "c6s", &type, &addr, &port) != n)
		return CHORD_PROTOCOL_ERROR;
	assert(type == CHORD_STAB);
	return process_stab(srv, &addr, port);
}

/**********************************************************************/

/* pack_stab_repl: pack stabilize reply packet */
int pack_stab_repl(uchar *buf, in6_addr *addr, ushort port)
{
	return pack(buf, "c6s", CHORD_STAB_REPL, addr, port);
}

/**********************************************************************/

/* unpack_stab_repl: unpack and process stabilize reply packet */
int unpack_stab_repl(Server *srv, int n, uchar *buf, Node *from)
{
	uchar type;
	in6_addr addr;
	ushort port;

	if (unpack(buf, "c6s", &type, &addr, &port) != n)
		return CHORD_PROTOCOL_ERROR;
	assert(type == CHORD_STAB_REPL);
	return process_stab_repl(srv, &addr, port);
}

/**********************************************************************/

/* pack_notify: pack notify packet */
int pack_notify(uchar *buf)
{
	return pack(buf, "c", CHORD_NOTIFY);
}

/**********************************************************************/

/* unpack_notify: unpack notify packet */
int unpack_notify(Server *srv, int n, uchar *buf, Node *from)
{
	uchar type;

	if (unpack(buf, "c", &type) != n)
		return CHORD_PROTOCOL_ERROR;
	assert(type == CHORD_NOTIFY);
	return process_notify(srv, from);
}

/**********************************************************************/

/* pack_ping: pack ping packet */
int pack_ping(uchar *buf, uchar *ticket, ulong time)
{
	return pack(buf, "ctl", CHORD_PING, ticket, time);
}

/**********************************************************************/

/* unpack_ping: unpack and process ping packet */
int unpack_ping(Server *srv, int n, uchar *buf, Node *from)
{
	uchar type;
	uchar ticket[TICKET_LEN];
	ulong time;

	if (unpack(buf, "ctl", &type, ticket, &time) != n)
		return CHORD_PROTOCOL_ERROR;

	assert(type == CHORD_PING);
	return process_ping(srv, ticket, time, from);
}

/**********************************************************************/

/* pack_pong: pack pong packet */
int pack_pong(uchar *buf, uchar *ticket, ulong time)
{
	return pack(buf, "ctl", CHORD_PONG, ticket, time);
}

/**********************************************************************/

/* unpack_pong: unpack pong packet */
int unpack_pong(Server *srv, int n, uchar *buf, Node *from)
{
	uchar type;
	uchar ticket[TICKET_LEN];
	ulong time;

	if (unpack(buf, "ctl", &type, ticket, &time) != n)
		return CHORD_PROTOCOL_ERROR;

	assert(type == CHORD_PONG);
	return process_pong(srv, ticket, time, from);
}

/**********************************************************************/

/* pack_fingers_get: pack get fingers packet */
int pack_fingers_get(uchar *buf, uchar *ticket, in6_addr *addr, ushort port,
					 chordID *key)
{
	return pack(buf, "ctx6s", CHORD_FINGERS_GET, ticket, key, addr, port);
}

/**********************************************************************/

/* unpack_fingers_get: unpack and process fingers_get packet */
int unpack_fingers_get(Server *srv, int n, uchar *buf, Node *from)
{
	uchar type;
	uchar ticket[TICKET_LEN];
	chordID key;
	in6_addr addr;
	ushort port;

	if (unpack(buf, "ctx6s", &type, ticket, &key, &addr, &port) != n)
		return CHORD_PROTOCOL_ERROR;
	assert(type == CHORD_FINGERS_GET);
	return process_fingers_get(srv, ticket, &addr, port, &key);
}

#define INCOMPLETE_FINGER_LIST -1
#define END_FINGER_LIST 0

/* pack_fingers_repl: pack repl fingers packet */
int pack_fingers_repl(uchar *buf, Server *srv, uchar *ticket)
{
	Finger *f;
	int len, l;

	assert(srv);
	len = pack(buf, "ctx6s", CHORD_FINGERS_REPL, ticket, &srv->node.id,
			   &srv->node.addr, srv->node.port);

	/* pack fingers */
	for (f = srv->head_flist; f; f = f->next) {
		l = pack(buf + len, "x6slls", &f->node.id, &f->node.addr, f->node.port,
				 f->rtt_avg, f->rtt_dev, (short)f->npings);
		len += l;
		if (len + l + 1 > BUFSIZE) {
			len += pack(buf + len, "c", INCOMPLETE_FINGER_LIST);
			return len;
		}
	}
	len += pack(buf + len, "c", END_FINGER_LIST);
	return len;
}


/* unpack_fingers_repl: unpack and process fingers_repl packet */
int unpack_fingers_repl(Server *srv, int n, uchar *buf, Node *from)
{
	/* this message is received by a client;
	 * it shouldn't be received by an i3 server
	 */
	return process_fingers_repl(srv, 0);
}


/**********************************************************************/

/* pack_traceroute: pack/update traceroute packet */
/*
 * traceroute packet format:
 *		char pkt_type;
 *		char ttl; time to live, decremented at every hop.
 *							When ttl reaches 0, a traceroute_repl packet is returned.
 *		char hops; number of hops up to the current node (not including the
 *							 client). hops is inceremented at every hop along the
 *							 forward path. hops should be initialized to 0 by the clients.
 *		ID target_id;	 target ID for traceroute.
 *		Node prev_node; previous node (ie., the node which forwarded the packet)
 *		ulong rtt; rtt...
 *		ulong dev; ... and std dev frm previous node to this node (in usec)
 *		Node crt_node; this node
 *		(list of addresses/ports of the nodes along the traceroute path
 *		 up to this node)
 *		ulong addr;	address...
 *		ushort port; ... and port number of the client
 *		....
 */
int pack_traceroute(uchar *buf, Server *srv, Finger *f, uchar type, byte ttl,
					byte hops)
{
	int	 n = 0;

	/* pack type, ttl and hops fields */
	n = pack(buf+n, "ccc", type, ttl, hops);

	/* skip target ID field */
	n += sizeof_fmt("x");

	/* pack prev node (for the next node, this is the current node!) */
	n += pack(buf+n, "x6sll", &srv->node.id, &srv->node.addr, srv->node.port,
			  f->rtt_avg, f->rtt_dev);

	/* pack this node field (for next node this is the node itself!) */
	n += pack(buf+n, "x6s", &f->node.id, &f->node.addr, f->node.port);

	/* skip current list of addresses .. */
	n += sizeof_fmt("6s")*hops;

	/* add current node to the list */
	n += pack(buf+n, "6s", &srv->node.addr, srv->node.port);

	return n;
}

/**********************************************************************/

/* unpack_traceroute: unpack and process trace_route packet
 * (see pack_traceroute for packet format)
 */
int unpack_traceroute(Server *srv, int n, uchar *buf, Node *from)
{
	chordID id;
	byte		type;
	byte		ttl;
	byte		hops;

	/* unpack following fields: type, ttl, hops, and target id */
	if (unpack(buf, "cccx", &type, &ttl, &hops, &id) >= n)
		return CHORD_PROTOCOL_ERROR;
	assert(type == CHORD_TRACEROUTE || type == CHORD_TRACEROUTE_LAST);
	return process_traceroute(srv, &id, buf, type, ttl, hops);
}

/**********************************************************************/

/* pack_traceroute_repl: pack/update traceroute packet */
/*
 * traceroute packet_repl format:
 *		char pkt_type;
 *		char ttl; time to live, incremented at every hop (remember, this message
 *							travls on the reverse path)
 *		char hops; number of hops from client, decremented at every
 *							 hop along the forward path. When hops reaches 0,
 *							 the packet is dropped.
 *		ID target_id;	 target ID for traceroute.
 *		Node nl_node; next-to-last hop on traceroute path
 *		ulong rtt; rtt...
 *		ulong dev; ... and std dev from next-to-last hop to the last hop
 *		Node l_node; last node of traceroute path
 *		(list of addresses/ports of the nodes along the traceroute path
 *		 up to this node, starting with the client)
 *		ulong addr;	address...
 *		ushort port; ... and port number of the client
 *		....
 */
int pack_traceroute_repl(uchar *buf, Server *srv, byte ttl, byte hops,
						 in6_addr *paddr, ushort *pport, int one_hop)
{
	int	 n = 0;

	/* pack ttl and hops fields */
	n += pack(buf+n, "ccc", CHORD_TRACEROUTE_REPL, (char)ttl, (char)hops);

	/* skip target ID field */
	n += sizeof_fmt("x");

	if (one_hop) {
		/* one hop path */
		n += pack(buf+n, "x6s", &srv->node.id, &srv->node.addr, srv->node.port);
		/* skip mean rtt, and std dev rtt fields */
		n += sizeof_fmt("ll");
	} else {
		/* skip next-to-last node field, and the mean and std dev. to last
		 * node fields
		 */
		n += sizeof_fmt("x6sll");
	}

	/* skip last node field */
	n += sizeof_fmt("x6s");

	/* compute source list size -- this is the number of hops plus client */
	n += sizeof_fmt("6s")*hops;

	/* get address and port number of previous node on reverse path
	 * yes, it's ugly to unpack in a pack function, but...
	 */
	unpack(buf+n, "6s", paddr, pport);

	return n;
}

/**********************************************************************/

/*	unpack_traceroute: unpack and process trace_route packet
 * (see pack_traceroute for packet format)
 */
int unpack_traceroute_repl(Server *srv, int n, uchar *buf, Node *from)
{
	byte type;
	byte ttl;
	byte hops;

	/* unpack following fields: type, ttl, and hops */
	if (unpack(buf, "ccc", &type, &ttl, &hops) >= n)
		return CHORD_PROTOCOL_ERROR;
	assert(type == CHORD_TRACEROUTE_REPL);

	return process_traceroute_repl(srv, buf, ttl, hops);
}
