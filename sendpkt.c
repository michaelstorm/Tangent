#include <assert.h>
#include <string.h>
#include "chord.h"

void send_addr_discover(Server *srv, in6_addr *to_addr, ushort to_port)
{
	byte buf[BUFSIZE];
	*(unsigned short *)buf = 0xFFFF;

	uchar ticket[TICKET_LEN];

	pack_ticket(&srv->ticket_key, ticket, "c6s", CHORD_ADDR_DISCOVER, to_addr,
				to_port);

	CHORD_DEBUG(5, print_send(srv, "send_addr_discover", 0, to_addr, to_port));
	send_packet(srv, to_addr, to_port, pack_addr_discover(buf+2, ticket)+2, buf);
}

void send_addr_discover_repl(Server *srv, uchar *ticket, in6_addr *to_addr,
							 ushort to_port)
{
	byte buf[BUFSIZE];
	*(unsigned short *)buf = 0xFFFF;

	CHORD_DEBUG(5, print_send(srv, "send_addr_discover_repl", 0, to_addr,
							  to_port));
	send_packet(srv, to_addr, to_port, pack_addr_discover_repl(buf+2, ticket,
															   to_addr)+2,
				buf);
}

void send_data(Server *srv, uchar type, byte ttl, Node *np, chordID *id,
			   ushort n, const uchar *data)
{
	byte buf[BUFSIZE];
	*(unsigned short *)buf = 0xFFFF;

	CHORD_DEBUG(3, print_send(srv, "send_data", id, &np->addr, np->port));
	send_packet(srv, &np->addr, np->port, pack_data(buf+2, type, ttl, id, n,
													data)+2, buf);
}

/**********************************************************************/

void send_fs(Server *srv, byte ttl, in6_addr *to_addr, ushort to_port,
			 in6_addr *addr, ushort port)
{
	byte buf[BUFSIZE];
	uchar ticket[TICKET_LEN];
	*(unsigned short *)buf = 0xFFFF;

	pack_ticket(&srv->ticket_key, ticket, "c", CHORD_FS);

	CHORD_DEBUG(5, print_send(srv, "send_fs", 0, to_addr, to_port));
	send_packet(srv, to_addr, to_port, pack_fs(buf+2, ticket, ttl, addr, port)+2,
				buf);
}

/**********************************************************************/

void send_fs_forward(Server *srv, uchar *ticket, byte ttl, in6_addr *to_addr,
					 ushort to_port, in6_addr *addr, ushort port)
{
	byte buf[BUFSIZE];
	*(unsigned short *)buf = 0xFFFF;

	CHORD_DEBUG(5, print_send(srv, "send_fs", 0, to_addr, to_port));
	send_packet(srv, to_addr, to_port, pack_fs(buf+2, ticket, ttl, addr, port)+2,
				buf);
}

/**********************************************************************/

void send_fs_repl(Server *srv, uchar *ticket, in6_addr *to_addr, ushort to_port,
				  in6_addr *addr, ushort port)
{
	byte buf[BUFSIZE];
	*(unsigned short *)buf = 0xFFFF;

	CHORD_DEBUG(5, print_send(srv, "send_fs_repl", 0, to_addr, to_port));
	send_packet(srv, to_addr, to_port, pack_fs_repl(buf+2, ticket, addr, port)+2,
				buf);
}

/**********************************************************************/

void send_stab(Server *srv, in6_addr *to_addr, ushort to_port, in6_addr *addr,
			   ushort port)
{
	byte buf[BUFSIZE];
	*(unsigned short *)buf = 0xFFFF;

	CHORD_DEBUG(5, print_send(srv, "send_stab", 0, to_addr, to_port));
	send_packet(srv, to_addr, to_port, pack_stab(buf+2, addr, port)+2, buf);
}

/**********************************************************************/

void send_stab_repl(Server *srv, in6_addr *to_addr, ushort to_port,
					in6_addr *addr, ushort port)
{
	byte buf[BUFSIZE];
	*(unsigned short *)buf = 0xFFFF;

	CHORD_DEBUG(5, print_send(srv, "send_stab_repl", 0, to_addr, to_port));
	send_packet(srv, to_addr, to_port, pack_stab_repl(buf+2, addr, port)+2, buf);
}

/**********************************************************************/

void send_notify(Server *srv, in6_addr *to_addr, ushort to_port)
{
	byte buf[BUFSIZE];
	*(unsigned short *)buf = 0xFFFF;

	CHORD_DEBUG(5, print_send(srv, "send_notify", 0, to_addr, to_port));
	send_packet(srv, to_addr, to_port, pack_notify(buf+2)+2, buf);
}

/**********************************************************************/

void send_ping(Server *srv, in6_addr *to_addr, ushort to_port, ulong time)
{
	byte buf[BUFSIZE];
	*(unsigned short *)buf = 0xFFFF;

	uchar ticket[TICKET_LEN];

	pack_ticket(&srv->ticket_key, ticket, "c6sl", CHORD_PING, to_addr,
				 to_port, time);

	CHORD_DEBUG(5, print_send(srv, "send_ping", &srv->node.id, to_addr,
							  to_port));
	send_packet(srv, to_addr, to_port, pack_ping(buf+2, ticket, time)+2, buf);
}

/**********************************************************************/

void send_pong(Server *srv, uchar *ticket, in6_addr *to_addr, ushort to_port,
			   ulong time)
{
	byte buf[BUFSIZE];
	*(unsigned short *)buf = 0xFFFF;

	CHORD_DEBUG(5, print_send(srv, "send_pong", &srv->node.id, to_addr,
							  to_port));
	send_packet(srv, to_addr, to_port, pack_pong(buf+2, ticket, time)+2, buf);
}


/**********************************************************************/

void send_fingers_get(Server *srv, in6_addr *to_addr, ushort to_port, in6_addr *addr,
					  ushort port, chordID *key)
{
	byte buf[BUFSIZE];
	*(unsigned short *)buf = 0xFFFF;

	uchar ticket[TICKET_LEN];

	pack_ticket(&srv->ticket_key, ticket, "c6s", CHORD_FINGERS_GET, to_addr,
				to_port);

	CHORD_DEBUG(5, print_send(srv, "send_fingers_get", NULL, to_addr, to_port));
	send_packet(srv, to_addr, to_port, pack_fingers_get(buf+2, ticket, addr, port,
													 key)+2, buf);
}

/**********************************************************************/

void send_fingers_repl(Server *srv, uchar *ticket, in6_addr *to_addr,
					   ushort to_port)
{
	byte buf[BUFSIZE];
	*(unsigned short *)buf = 0xFFFF;

	CHORD_DEBUG(5, print_send(srv, "send_fingers_repl", &srv->node.id, to_addr,
							to_port));
	send_packet(srv, to_addr, to_port, pack_fingers_repl(buf+2, srv, ticket)+2, buf);
}

/**********************************************************************/

void send_traceroute(Server *srv, Finger *f, uchar *buf, uchar type, byte ttl,
					 byte hops)
{
	CHORD_DEBUG(5, print_send(srv, "send_traceroute", &srv->node.id, NULL, -1));
	send_packet(srv, &f->node.addr, f->node.port, pack_traceroute(buf+2, srv, f,
															   type, ttl, hops)+2,
			 buf);
}

/**********************************************************************/

void send_traceroute_repl(Server *srv, uchar *buf, int ttl, int hops,
						  int one_hop)
{
	in6_addr *to_addr;
	ushort to_port;

	CHORD_DEBUG(5, print_send(srv, "send_traceroute_repl", &srv->node.id, NULL,
							-1));
	send_packet(srv, to_addr, to_port,
				pack_traceroute_repl(buf+2, srv, ttl, hops, to_addr, &to_port,
									 one_hop)+2,
				buf);
}

/**********************************************************************/

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

	if (sendto(sock, buf, n, 0, (struct sockaddr *) &dest, sizeof(dest)) < 0)
		weprintf("sendto failed:"); /* ignore errors for now */
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
