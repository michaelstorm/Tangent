#include <string.h>
#include <unistd.h>
#include "chord.h"
#include "dhash.h"
#include "pack.h"
#include "send.h"
#include "transfer.h"

/* Send file query result to the control process
 */
void dhash_send_control_packet(DHash *dhash, int code, const char *file)
{
	uchar buf[1024];
	int n = dhash_pack_control_request_reply(buf, code, file, strlen(file));

	if (write(dhash->control_sock, buf, n) < 0)
		perror("writing control packet");
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
void dhash_send_query(DHash *dhash, const char *file)
{
	fprintf(stderr, "sending file query for %s\n", file);

	uchar buf[1024];
	int n;
	short file_len = strlen(file);

	chordID id;
	get_data_id(&id, (const uchar *)file, file_len);

	int i;
	for (i = 0; i < dhash->nservers; i++) {
		Server *srv = dhash->servers[i];

		/* pack the server's reply address and port */
		n = dhash_pack_query(buf, &srv->node.addr, srv->node.port, file,
							 file_len);

		/* send it ourselves rather than tunneling, to avoid having it echoed
		   back to us */
		uchar route_type;
		Node *next = next_route_node(srv, &id, CHORD_ROUTE, &route_type);
		if (!IN6_IS_ADDR_UNSPECIFIED(&next->addr))
			send_data(srv, route_type, 10, next, &id, n, buf);
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
	send_data(srv, CHORD_ROUTE, 10, &node, &id, n, data);
}

/* Notify the requesting node that we have the file.
 */
void dhash_send_query_reply_success(DHash *dhash, Server *srv, in6_addr *addr,
									ushort port, const char *file)
{
	fprintf(stderr, "sending query reply SUCCESS for %s to [%s]:%d\n", file,
		   v6addr_to_str(addr), port);

	uchar buf[1024];
	int file_size = dhash_local_file_size(dhash, file);
	int n = dhash_pack_query_reply_success(buf, file_size, file, strlen(file));
	send_chord_pkt_directly(srv, addr, port, buf, n);
}

/* Notify the requesting node that we are the file's successor, but don't have
   it.
 */
void dhash_send_query_reply_failure(DHash *dhash, Server *srv, in6_addr *addr,
									ushort port, const char *file)
{
	fprintf(stderr, "sending query reply FAILURE for %s to [%s]:%d\n", file,
		   v6addr_to_str(addr), port);

	uchar buf[1024];
	int n = dhash_pack_query_reply_failure(buf, file, strlen(file));
	send_chord_pkt_directly(srv, addr, port, buf, n);
}

/* Notify the control process that a file was downloaded.
 */
void dhash_send_control_transfer_complete(DHash *dhash, Transfer *trans)
{
	dhash_send_control_packet(dhash, DHASH_CLIENT_REPLY_SUCCESS, trans->file);
}

/* Notify the control process that a file could not be downloaded.
 */
void dhash_send_control_transfer_failed(DHash *dhash, Transfer *trans)
{
	dhash_send_control_packet(dhash, DHASH_CLIENT_REPLY_FAILURE, trans->file);
}

/* Send a file request to the DHash process.
 */
void dhash_client_send_request(int sock, const char *file)
{
	short size = strlen(file);
	uchar buf[sizeof_fmt("s") + size];

	int n = pack(buf, "cs", DHASH_CLIENT_QUERY, size);
	memcpy(buf + n, file, size);
	n += size;

	if (write(sock, buf, n) < 0)
		perror("writing file request");
}
