#include <string.h>
#include <unistd.h>
#include "chord.h"
#include "dhash.h"
#include "send.h"

void dhash_send_control_packet(DHash *dhash, int code, const char *file)
{
	short size = strlen(file);
	uchar buf[sizeof_fmt("s") + size];

	int n = pack(buf, "cs", code, size);
	memcpy(buf + n, file, size);
	n += size;

	if (write(dhash->control_sock, buf, n) < 0)
		perror("writing control packet");
}

void dhash_send_file_query(DHash *dhash, const char *file)
{
	printf("sending file query for %s\n", file);

	uchar buf[1024];

	short size = strlen(file);
	int data_len = pack(buf, "c*6*ss", DHASH_QUERY, size);
	memcpy(buf + data_len, file, size);

	chordID id;
	get_data_id(&id, (const uchar *)file, size);

	int i;
	for (i = 0; i < dhash->nservers; i++) {
		Server *srv = dhash->servers[i];

		/* pack the server's reply address and port */
		pack(buf, "*c6s", &srv->node.addr, srv->node.port);

		/* send it ourselves rather than tunneling to avoid having it echoed
		   back to us */
		uchar route_type;
		Node *next = next_route_node(srv, &id, CHORD_ROUTE, &route_type);
		if (!IN6_IS_ADDR_UNSPECIFIED(&next->addr))
			send_data(srv, route_type, 10, next, &id, data_len + size, buf);
	}
}

void dhash_send_query_reply_success(DHash *dhash, Server *srv, in6_addr *addr,
									ushort port, const char *file)
{
	printf("sending query reply SUCCESS for %s to [%s]:%d\n", file,
		   v6addr_to_str(addr), port);

	uchar buf[1024];

	ushort name_len = strlen(file);
	int file_size = dhash_local_file_size(dhash, file);
	int data_len = pack(buf, "cls", DHASH_QUERY_REPLY_SUCCESS, file_size,
						name_len);
	memcpy(buf + data_len, file, name_len);

	chordID id;
	get_address_id(&id, addr, port);

	Node node;
	v6_addr_copy(&node.addr, addr);
	node.port = port;
	send_data(srv, CHORD_ROUTE, 10, &node, &id, data_len + name_len, buf);
}

void dhash_send_query_reply_failure(DHash *dhash, Server *srv, in6_addr *addr,
									ushort port, const char *file)
{
	printf("sending query reply SUCCESS for %s to [%s]:%d\n", file,
		   v6addr_to_str(addr), port);

	uchar buf[1024];

	ushort name_len = strlen(file);
	int data_len = pack(buf, "cs", DHASH_QUERY_REPLY_FAILURE, name_len);
	memcpy(buf + data_len, file, name_len);

	chordID id;
	get_address_id(&id, addr, port);

	Node node;
	v6_addr_copy(&node.addr, addr);
	node.port = port;
	send_data(srv, CHORD_ROUTE, 10, &node, &id, data_len + name_len, buf);
}

int dhash_send_control_transfer_complete(DHash *dhash, Transfer *trans,
										 void *ctx, int event)
{
	printf("dhash_send_control_transfer_complete\n");
	dhash_remove_transfer(dhash, trans);
}

int dhash_send_control_transfer_failed(DHash *dhash, Transfer *trans, void *ctx,
									   int event)
{
	printf("dhash_send_control_transfer_failed\n");
	dhash_remove_transfer(dhash, trans);
}
