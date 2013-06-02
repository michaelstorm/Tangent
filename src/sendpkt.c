#include <assert.h>
#include <string.h>
#include "chord/chord.h"
#include "chord/crypt.h"
#include "chord/pack.h"
#include "chord/sendpkt.h"
#include "chord/util.h"

static uchar ticket_buf[1024];

#define LOG_SEND() \
{ \
	StartLog(TRACE); \
	print_send(clog_file_logger()->fp, srv, (char *)__func__, 0, to_addr, to_port); \
	EndLog(); \
}

static uchar buf[BUFSIZE];

void send_addr_discover(Server *srv, in6_addr *to_addr, ushort to_port)
{
	int ticket_len = pack_ticket(srv->ticket_salt, srv->ticket_salt_len,
								 srv->ticket_hash_len, ticket_buf, "c6s",
								 CHORD_ADDR_DISCOVER, to_addr, to_port);

	int len = pack_addr_discover(buf, ticket_buf, ticket_len);

	LOG_SEND();
	send_packet(srv, to_addr, to_port, len, buf);
}

void send_addr_discover_reply(Server *srv, uchar *ticket, int ticket_len,
							  in6_addr *to_addr, ushort to_port)
{
	int len = pack_addr_discover_reply(buf, ticket, ticket_len, to_addr);

	LOG_SEND();
	send_packet(srv, to_addr, to_port, len, buf);
}

void send_data(Server *srv, int last, uchar ttl, Node *np, chordID *id,
			   ushort n, const uchar *data)
{
	in6_addr *to_addr = &np->addr;
	ushort to_port = np->port;

	int len = pack_data(buf, last, ttl, id, n, data);

	LOG_SEND();
	send_packet(srv, &np->addr, np->port, len, buf);
}

/**********************************************************************/

void send_fs(Server *srv, uchar ttl, in6_addr *to_addr, ushort to_port,
			 in6_addr *addr, ushort port)
{
	int ticket_len = pack_ticket(srv->ticket_salt, srv->ticket_salt_len,
								 srv->ticket_hash_len, ticket_buf, "c",
								 CHORD_FS);

	int len = pack_fs(buf, ticket_buf, ticket_len, ttl, addr, port);

	LOG_SEND();
	send_packet(srv, to_addr, to_port, len, buf);
}

/**********************************************************************/

void send_fs_forward(Server *srv, uchar *ticket, int ticket_len, uchar ttl,
					 in6_addr *to_addr, ushort to_port, in6_addr *addr,
					 ushort port)
{
	int len = pack_fs(buf, ticket, ticket_len, ttl, addr, port);

	LOG_SEND();
	send_packet(srv, to_addr, to_port, len, buf);
}

/**********************************************************************/

void send_fs_reply(Server *srv, uchar *ticket, int ticket_len,
				   in6_addr *to_addr, ushort to_port, in6_addr *addr,
				   ushort port)
{
	int len = pack_fs_reply(buf, ticket, ticket_len, addr, port);

	LOG_SEND();
	send_packet(srv, to_addr, to_port, len, buf);
}

/**********************************************************************/

void send_stab(Server *srv, in6_addr *to_addr, ushort to_port, in6_addr *addr,
			   ushort port)
{
	int len = pack_stab(buf, addr, port);
	
	LOG_SEND();
	send_packet(srv, to_addr, to_port, len, buf);
}

/**********************************************************************/

void send_stab_reply(Server *srv, in6_addr *to_addr, ushort to_port,
					in6_addr *addr, ushort port)
{
	int len = pack_stab_reply(buf, addr, port);
	
	LOG_SEND();
	send_packet(srv, to_addr, to_port, len, buf);
}

/**********************************************************************/

void send_notify(Server *srv, in6_addr *to_addr, ushort to_port)
{
	int len = pack_notify(buf);
	
	LOG_SEND();
	send_packet(srv, to_addr, to_port, len, buf);
}

/**********************************************************************/

void send_ping(Server *srv, in6_addr *to_addr, ushort to_port, ulong time)
{
	int ticket_len = pack_ticket(srv->ticket_salt, srv->ticket_salt_len,
								 srv->ticket_hash_len, ticket_buf, "c6sl",
								 CHORD_PING, to_addr, to_port, time);

	int len = pack_ping(buf, ticket_buf, ticket_len, time);

	LOG_SEND();
	send_packet(srv, to_addr, to_port, len, buf);
}

/**********************************************************************/

void send_pong(Server *srv, uchar *ticket, int ticket_len, in6_addr *to_addr,
			   ushort to_port, ulong time)
{
	int len = pack_pong(buf, ticket, ticket_len, time);

	LOG_SEND();
	send_packet(srv, to_addr, to_port, len, buf);
}

/* send_packet: send datagram to remote addr:port */
void send_packet(Server *srv, in6_addr *addr, in_port_t port, int n, uchar *buf)
{	
	srv->send_func(srv->sock, addr, port, n, buf);
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
