#include <assert.h>
#include <string.h>
#include "chord.h"

static uchar ticket_buf[1024];

#ifdef CHORD_MESSAGE_DEBUG
#define CHORD_PRINT_MESSAGE_DEBUG \
	fprintf(stderr, "===> "); \
	chordID to_id; \
	get_address_id(&to_id, to_addr, to_port); \
	print_chordID(&to_id); \
	fprintf(stderr, " ([%s]:%d)\n", v6addr_to_str(to_addr), to_port);
#else
#define CHORD_PRINT_MESSAGE_DEBUG
#endif

#define LOG_SEND_LEVEL LOG_LEVEL_TRACE

#define LOG_SEND \
{ \
	LinkedString *str = lstr_empty(); \
	log_send(str, srv, __func__, 0, to_addr, to_port); \
	LogString(TRACE, str); \
	lstr_free(str); \
}

void send_addr_discover(Server *srv, in6_addr *to_addr, ushort to_port)
{
	CHORD_PRINT_MESSAGE_DEBUG
	uchar buf[BUFSIZE];

	int ticket_len = pack_ticket(srv->ticket_salt, srv->ticket_salt_len,
								 srv->ticket_hash_len, ticket_buf, "c6s",
								 CHORD_ADDR_DISCOVER, to_addr, to_port);

	LOG_SEND;
	send_packet(srv, to_addr, to_port, pack_addr_discover(buf, ticket_buf,
														  ticket_len),
				buf);
}

void send_addr_discover_reply(Server *srv, uchar *ticket, int ticket_len,
							  in6_addr *to_addr, ushort to_port)
{
	CHORD_PRINT_MESSAGE_DEBUG
	uchar buf[BUFSIZE];

	LOG_SEND;
	send_packet(srv, to_addr, to_port, pack_addr_discover_reply(buf, ticket,
																ticket_len,
																to_addr),
				buf);
}

void send_data(Server *srv, int last, uchar ttl, Node *np, chordID *id,
			   ushort n, const uchar *data)
{
	in6_addr *to_addr = &np->addr;
	ushort to_port = np->port;
	CHORD_PRINT_MESSAGE_DEBUG
	uchar buf[BUFSIZE];

	LOG_SEND;
	
	send_packet(srv, &np->addr, np->port, pack_data(buf, last, ttl, id, n,
													data), buf);
}

/**********************************************************************/

void send_fs(Server *srv, uchar ttl, in6_addr *to_addr, ushort to_port,
			 in6_addr *addr, ushort port)
{
	CHORD_PRINT_MESSAGE_DEBUG
	uchar buf[BUFSIZE];

	int ticket_len = pack_ticket(srv->ticket_salt, srv->ticket_salt_len,
								 srv->ticket_hash_len, ticket_buf, "c",
								 CHORD_FS);

	LOG_SEND;
	send_packet(srv, to_addr, to_port, pack_fs(buf, ticket_buf, ticket_len, ttl,
											   addr, port),
				buf);
}

/**********************************************************************/

void send_fs_forward(Server *srv, uchar *ticket, int ticket_len, uchar ttl,
					 in6_addr *to_addr, ushort to_port, in6_addr *addr,
					 ushort port)
{
	CHORD_PRINT_MESSAGE_DEBUG
	uchar buf[BUFSIZE];

	LOG_SEND;
	send_packet(srv, to_addr, to_port, pack_fs(buf, ticket, ticket_len, ttl,
											   addr, port),
				buf);
}

/**********************************************************************/

void send_fs_reply(Server *srv, uchar *ticket, int ticket_len,
				   in6_addr *to_addr, ushort to_port, in6_addr *addr,
				   ushort port)
{
	CHORD_PRINT_MESSAGE_DEBUG
	uchar buf[BUFSIZE];

	LOG_SEND;
	send_packet(srv, to_addr, to_port, pack_fs_reply(buf, ticket, ticket_len,
													 addr, port),
				buf);
}

/**********************************************************************/

void send_stab(Server *srv, in6_addr *to_addr, ushort to_port, in6_addr *addr,
			   ushort port)
{
	CHORD_PRINT_MESSAGE_DEBUG
	uchar buf[BUFSIZE];

	LOG_SEND;
	send_packet(srv, to_addr, to_port, pack_stab(buf, addr, port), buf);
}

/**********************************************************************/

void send_stab_reply(Server *srv, in6_addr *to_addr, ushort to_port,
					in6_addr *addr, ushort port)
{
	CHORD_PRINT_MESSAGE_DEBUG
	uchar buf[BUFSIZE];

	LOG_SEND;
	send_packet(srv, to_addr, to_port, pack_stab_reply(buf, addr, port), buf);
}

/**********************************************************************/

void send_notify(Server *srv, in6_addr *to_addr, ushort to_port)
{
	CHORD_PRINT_MESSAGE_DEBUG
	uchar buf[BUFSIZE];

	LOG_SEND;
	send_packet(srv, to_addr, to_port, pack_notify(buf), buf);
}

/**********************************************************************/

void send_ping(Server *srv, in6_addr *to_addr, ushort to_port, ulong time)
{
	CHORD_PRINT_MESSAGE_DEBUG
	uchar buf[BUFSIZE];

	int ticket_len = pack_ticket(srv->ticket_salt, srv->ticket_salt_len,
								 srv->ticket_hash_len, ticket_buf, "c6sl",
								 CHORD_PING, to_addr, to_port, time);

	LOG_SEND;
	send_packet(srv, to_addr, to_port, pack_ping(buf, ticket_buf, ticket_len,
												 time),
				buf);
}

/**********************************************************************/

void send_pong(Server *srv, uchar *ticket, int ticket_len, in6_addr *to_addr,
			   ushort to_port, ulong time)
{
	CHORD_PRINT_MESSAGE_DEBUG
	uchar buf[BUFSIZE];

	LOG_SEND;
	send_packet(srv, to_addr, to_port, pack_pong(buf, ticket, ticket_len, time),
				buf);
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
