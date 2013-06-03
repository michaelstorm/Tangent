#ifndef SENDPKT_H
#define SENDPKT_H

#ifdef __cplusplus
extern "C" {
#endif

void send_packet(ChordServer *srv, in6_addr *addr, in_port_t port, int n,
				 uchar *buf);
void send_raw_v4(int sock, in6_addr *addr, in_port_t port, int n, uchar *buf);
void send_raw_v6(int sock, in6_addr *addr, in_port_t port, int n, uchar *buf);
void send_data(ChordServer *srv, int last, uchar ttl, Node *np, chordID *id,
			   ushort n, const uchar *data);
void send_fs(ChordServer *srv, uchar ttl, in6_addr *to_addr, ushort to_port,
			 in6_addr *addr, ushort port);
void send_fs_forward(ChordServer *srv, uchar *ticket, int ticket_len, uchar ttl,
					 in6_addr *to_addr, ushort to_port, in6_addr *addr,
					 ushort port);
void send_fs_reply(ChordServer *srv, uchar *ticket, int ticket_len,
				   in6_addr *to_addr, ushort to_port, in6_addr *addr,
				   ushort port);
void send_stab(ChordServer *srv, in6_addr *to_addr, ushort to_port, in6_addr *addr,
			   ushort port);
void send_stab_reply(ChordServer *srv, in6_addr *to_addr, ushort to_port,
					in6_addr *addr, ushort port);
void send_notify(ChordServer *srv, in6_addr *to_addr, ushort to_port);
void send_ping(ChordServer *srv, in6_addr *to_addr, ushort to_port, ulong time);
void send_pong(ChordServer *srv, uchar *ticket, int ticket_len, in6_addr *to_addr,
			   ushort to_port, ulong time);
void send_addr_discover(ChordServer *srv, in6_addr *to_addr, ushort to_port);
void send_addr_discover_reply(ChordServer *srv, uchar *ticket, int ticket_len,
							  in6_addr *to_addr, ushort to_port);

#ifdef __cplusplus
}
#endif

#endif
