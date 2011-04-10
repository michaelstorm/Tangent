#include <string.h>
#include <unistd.h>
#include "chord.h"
#include "dhash.h"
#include "pack.h"
#include "send.h"
#include "transfer.h"

void dhash_send_push(DHash *dhash, const uchar *name, int name_len)
{
	fprintf(stderr, "sending push for \"%s\"\n", buf_to_str(name, name_len));

	uchar buf[1024];
	int n;

	chordID id;
	get_data_id(&id, (const uchar *)name, name_len);

	int i;
	for (i = 0; i < dhash->nservers; i++) {
		Server *srv = dhash->servers[i];

		/* pack the server's reply address and port */
		n = dhash_pack_push(buf, &srv->node.addr, srv->node.port, name,
							name_len);

		/* send it ourselves rather than tunneling, to avoid having it echoed
		   back to us */
		int next_is_last;
		Node *next = next_route_node(srv, &id, 0, &next_is_last);
		if (!IN6_IS_ADDR_UNSPECIFIED(&next->addr))
			send_data(srv, next_is_last, 10, next, &id, n, buf);
	}
}

/* Send a file request to the network. This is accomplished by addressing a
   Chord message to the successor of the SHA-1 hash of the file name (without
   the terminating NUL character). We then send the message to the closest
   finger that is a predecessor of the hash, just like normal Chord routing. We
   do it ourselves at the DHash layer, though, to avoid having the message
   echoed back to us if we are the hash's successor. This could probably be
   better accomplished by sending it over the Chord tunnel after testing whether
   the id is local.
 */
void dhash_send_query(DHash *dhash, const uchar *name, int name_len)
{
	fprintf(stderr, "sending query for \"%s\"\n", buf_to_str(name, name_len));

	uchar buf[1024];
	int n;

	chordID id;
	get_data_id(&id, (const uchar *)name, name_len);

	int i;
	for (i = 0; i < dhash->nservers; i++) {
		Server *srv = dhash->servers[i];

		/* pack the server's reply address and port */
		n = dhash_pack_query(buf, &srv->node.addr, srv->node.port, name,
							 name_len);

		/* send it ourselves rather than tunneling, to avoid having it echoed
		   back to us */
		int next_is_last;
		Node *next = next_route_node(srv, &id, 0, &next_is_last);
		if (!IN6_IS_ADDR_UNSPECIFIED(&next->addr))
			send_data(srv, next_is_last, 10, next, &id, n, buf);
	}
}

/* Wrap a message in the Chord data header, but bypass Chord routing and send it
   to the node directly. An unfriendly NAT could drop the packets, though, so we
   might want to introduce a fallback mechanism that routes them over Chord as
   well, at the cost of increased network load and latency for queries.
 */
static void send_chord_pkt_directly(Server *srv, in6_addr *addr, ushort port,
									const uchar *data, int n)
{
	chordID id;
	get_address_id(&id, addr, port);

	Node node;
	v6_addr_copy(&node.addr, addr);
	node.port = port;
	send_data(srv, 0, 10, &node, &id, n, data);
}

/* Notify the requesting node that we have the file.
 */
void dhash_send_query_reply_success(DHash *dhash, Server *srv, in6_addr *addr,
									ushort port, const uchar *name, int name_len)
{
	fprintf(stderr, "sending query reply SUCCESS to [%s]:%d\n",
		   v6addr_to_str(addr), port);

	uchar buf[1024];
	int n = dhash_pack_query_reply_success(buf, name, name_len);
	send_chord_pkt_directly(srv, addr, port, buf, n);
}

/* Notify the requesting node that we are the file's successor, but don't have
   it.
 */
void dhash_send_query_reply_failure(DHash *dhash, Server *srv, in6_addr *addr,
									ushort port, const uchar *name, int name_len)
{
	fprintf(stderr, "sending query reply FAILURE to [%s]:%d\n",
			v6addr_to_str(addr), port);

	uchar buf[1024];
	int n = dhash_pack_query_reply_failure(buf, name, name_len);
	send_chord_pkt_directly(srv, addr, port, buf, n);
}

void dhash_send_push_reply(DHash *dhash, Server *srv, in6_addr *addr,
						   ushort port, const uchar *name, int name_len)
{
	fprintf(stderr, "sending push reply to [%s]:%d\n", v6addr_to_str(addr),
			port);

	uchar buf[1024];
	int n = dhash_pack_push_reply(buf, name, name_len);
	send_chord_pkt_directly(srv, addr, port, buf, n);
}

/* Send file query result to the control process.
 */
void dhash_send_control_packet(DHash *dhash, int code, const uchar *name,
							   int name_len)
{
	uchar buf[1024];
	int n = dhash_pack_control_request_reply(buf, code, name, name_len);

	if (write(dhash->control_sock, buf, n) < 0)
		perror("writing control packet");
}

/* Notify the control process that a file was downloaded.
 */
void dhash_send_control_query_success(DHash *dhash, const uchar *name,
									  int name_len)
{
	dhash_send_control_packet(dhash, DHASH_CLIENT_REPLY_SUCCESS, name,
							  name_len);
}

/* Notify the control process that a file could not be downloaded.
 */
void dhash_send_control_query_failure(DHash *dhash, const uchar *name,
									  int name_len)
{
	dhash_send_control_packet(dhash, DHASH_CLIENT_REPLY_FAILURE, name,
							  name_len);
}

/* Send a file request to the DHash process.
 */
void dhash_client_send_request(int sock, const uchar *name, int name_len)
{
	uchar buf[BUFSIZE];
	int n = dhash_pack_client_request(buf, name, name_len);

	if (write(sock, buf, n) < 0)
		perror("writing name request");
}
