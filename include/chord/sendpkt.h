#ifndef SENDPKT_H
#define SENDPKT_H

#ifdef __cplusplus
extern "C" {
#endif

void send_packet(Server *srv, in6_addr *addr, in_port_t port, int n,
				 uchar *buf);
void send_raw_v4(int sock, in6_addr *addr, in_port_t port, int n, uchar *buf);
void send_raw_v6(int sock, in6_addr *addr, in_port_t port, int n, uchar *buf);
void send_data(Server *srv, int last, uchar ttl, Node *np, chordID *id,
			   ushort n, const uchar *data);
void send_fs(Server *srv, uchar ttl, in6_addr *to_addr, ushort to_port,
			 in6_addr *addr, ushort port);
void send_fs_forward(Server *srv, uchar *ticket, int ticket_len, uchar ttl,
					 in6_addr *to_addr, ushort to_port, in6_addr *addr,
					 ushort port);
void send_fs_reply(Server *srv, uchar *ticket, int ticket_len,
				   in6_addr *to_addr, ushort to_port, in6_addr *addr,
				   ushort port);
void send_stab(Server *srv, in6_addr *to_addr, ushort to_port, in6_addr *addr,
			   ushort port);
void send_stab_reply(Server *srv, in6_addr *to_addr, ushort to_port,
					in6_addr *addr, ushort port);
void send_notify(Server *srv, in6_addr *to_addr, ushort to_port);
void send_ping(Server *srv, in6_addr *to_addr, ushort to_port, ulong time);
void send_pong(Server *srv, uchar *ticket, int ticket_len, in6_addr *to_addr,
			   ushort to_port, ulong time);
void send_addr_discover(Server *srv, in6_addr *to_addr, ushort to_port);
void send_addr_discover_reply(Server *srv, uchar *ticket, int ticket_len,
							  in6_addr *to_addr, ushort to_port);

#ifdef __cplusplus
}
#endif

#endif