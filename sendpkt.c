#include <assert.h>
#include <string.h>
#include "chord.h"

void send_addr_discover(Server *srv, in6_addr *to_addr, ushort to_port)
{
	uchar buf[BUFSIZE];
	uchar ticket[TICKET_LEN];

	pack_ticket(&srv->ticket_key, ticket, "c6s", CHORD_ADDR_DISCOVER, to_addr,
				to_port);

	CHORD_DEBUG(5, print_send(srv, "send_addr_discover", 0, to_addr, to_port));
	send_packet(srv, to_addr, to_port, pack_addr_discover(buf, ticket), buf);
}

void send_addr_discover_reply(Server *srv, uchar *ticket, in6_addr *to_addr,
							 ushort to_port)
{
	uchar buf[BUFSIZE];

	CHORD_DEBUG(5, print_send(srv, "send_addr_discover_repl", 0, to_addr,
							  to_port));
	send_packet(srv, to_addr, to_port, pack_addr_discover_reply(buf, ticket,
															   to_addr),
				buf);
}

void send_data(Server *srv, uchar type, uchar ttl, Node *np, chordID *id,
			   ushort n, const uchar *data)
{
	uchar buf[BUFSIZE];

	CHORD_DEBUG(3, print_send(srv, "send_data", id, &np->addr, np->port));
	send_packet(srv, &np->addr, np->port, pack_data(buf, type, ttl, id, n,
													data), buf);
}

/**********************************************************************/

void send_fs(Server *srv, uchar ttl, in6_addr *to_addr, ushort to_port,
			 in6_addr *addr, ushort port)
{
	uchar buf[BUFSIZE];
	uchar ticket[TICKET_LEN];

	pack_ticket(&srv->ticket_key, ticket, "c", CHORD_FS);

	CHORD_DEBUG(5, print_send(srv, "send_fs", 0, to_addr, to_port));
	send_packet(srv, to_addr, to_port, pack_fs(buf, ticket, ttl, addr, port),
				buf);
}

/**********************************************************************/

void send_fs_forward(Server *srv, uchar *ticket, uchar ttl, in6_addr *to_addr,
					 ushort to_port, in6_addr *addr, ushort port)
{
	uchar buf[BUFSIZE];

	CHORD_DEBUG(5, print_send(srv, "send_fs", 0, to_addr, to_port));
	send_packet(srv, to_addr, to_port, pack_fs(buf, ticket, ttl, addr, port),
				buf);
}

/**********************************************************************/

void send_fs_reply(Server *srv, uchar *ticket, in6_addr *to_addr, ushort to_port,
				  in6_addr *addr, ushort port)
{
	uchar buf[BUFSIZE];

	CHORD_DEBUG(5, print_send(srv, "send_fs_repl", 0, to_addr, to_port));
	send_packet(srv, to_addr, to_port, pack_fs_reply(buf, ticket, addr, port),
				buf);
}

/**********************************************************************/

void send_stab(Server *srv, in6_addr *to_addr, ushort to_port, in6_addr *addr,
			   ushort port)
{
	uchar buf[BUFSIZE];

	CHORD_DEBUG(5, print_send(srv, "send_stab", 0, to_addr, to_port));
	send_packet(srv, to_addr, to_port, pack_stab(buf, addr, port), buf);
}

/**********************************************************************/

void send_stab_reply(Server *srv, in6_addr *to_addr, ushort to_port,
					in6_addr *addr, ushort port)
{
	uchar buf[BUFSIZE];

	CHORD_DEBUG(5, print_send(srv, "send_stab_repl", 0, to_addr, to_port));
	send_packet(srv, to_addr, to_port, pack_stab_reply(buf, addr, port), buf);
}

/**********************************************************************/

void send_notify(Server *srv, in6_addr *to_addr, ushort to_port)
{
	uchar buf[BUFSIZE];

	CHORD_DEBUG(5, print_send(srv, "send_notify", 0, to_addr, to_port));
	send_packet(srv, to_addr, to_port, pack_notify(buf), buf);
}

/**********************************************************************/

void send_ping(Server *srv, in6_addr *to_addr, ushort to_port, ulong time)
{
	uchar buf[BUFSIZE];
	uchar ticket[TICKET_LEN];

	pack_ticket(&srv->ticket_key, ticket, "c6sl", CHORD_PING, to_addr,
				 to_port, time);

	CHORD_DEBUG(5, print_send(srv, "send_ping", &srv->node.id, to_addr,
							  to_port));
	send_packet(srv, to_addr, to_port, pack_ping(buf, ticket, time), buf);
}

/**********************************************************************/

void send_pong(Server *srv, uchar *ticket, in6_addr *to_addr, ushort to_port,
			   ulong time)
{
	uchar buf[BUFSIZE];

	CHORD_DEBUG(5, print_send(srv, "send_pong", &srv->node.id, to_addr,
							  to_port));
	send_packet(srv, to_addr, to_port, pack_pong(buf, ticket, time), buf);
}

/* send_packet: send datagram to remote addr:port */
void send_packet(Server *srv, in6_addr *addr, in_port_t port, int n, uchar *buf)
{
	if (srv->is_v6)
		send_raw_v6(srv->sock, addr, port, n, buf);
	else
		send_raw_v4(srv->sock, addr, port, n, buf);
}

void send_raw_v4(int sock, in6_addr *addr, in_port_t port, int n, uchar *buf)
{
	struct sockaddr_in dest;
	assert(V4_MAPPED(addr));

	memset(&dest, 0, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(port);
	dest.sin_addr.s_addr = to_v4addr(addr);

	if (sendto(sock, buf, n, 0, (struct sockaddr *)&dest, sizeof(dest)) < 0)
		weprintf("sendto failed:");
}

void send_raw_v6(int sock, in6_addr *addr, in_port_t port, int n, uchar *buf)
{
	struct sockaddr_in6 dest;

	memset(&dest, 0, sizeof(dest));
	dest.sin6_family = AF_INET6;
	dest.sin6_port = htons(port);
	v6_addr_copy(&dest.sin6_addr, addr);

	if (sendto(sock, buf, n, 0, (struct sockaddr *)&dest, sizeof(dest)) < 0)
		weprintf("sendto failed:");
}
