#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <udt>
#include <unistd.h>
#include "chord.h"
#include "dhash.h"

int dhash_handle_udt_packet(DHash *dhash, int sock)
{
	uchar type;
	if (recv(sock, &type, 1, MSG_PEEK) == 1 && (type >> 4) == 0x0F)
		return 0;

	printf("received udt handshake\n");
}

DHash *new_dhash(const char *files_path)
{
	DHash *dhash = (DHash *)malloc(sizeof(DHash));
	dhash->servers = NULL;
	dhash->nservers = 0;

	dhash->files_path = (char *)malloc(strlen(files_path)+1);
	strcpy(dhash->files_path, files_path);
	return dhash;
}

int dhash_local_file_exists(DHash *dhash, const char *file)
{
	struct stat stat_buf;
	char abs_file_path[strlen(dhash->files_path) + strlen(file) + 1];

	strcpy(abs_file_path, dhash->files_path);
	strcpy(abs_file_path + strlen(dhash->files_path), "/");
	strcpy(abs_file_path + strlen(dhash->files_path)+1, file);

	if (stat(abs_file_path, &stat_buf) == 0)
		return 1;
	else
		return 0;
}

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

void dhash_send_file_query_reply(Server *srv, in6_addr *addr, ushort port,
								 int code, const char *file)
{
	printf("sending file query reply type %d for %s to [%s]:%d\n", code, file,
		   v6addr_to_str(addr), port);

	uchar buf[1024];

	short size = strlen(file);
	int data_len = pack(buf, "cs", code, size);
	memcpy(buf + data_len, file, size);

	chordID id;
	get_address_id(&id, addr, port);

	Node node;
	v6_addr_copy(&node.addr, addr);
	node.port = port;
	send_data(srv, CHORD_ROUTE, 10, &node, &id, data_len + size, buf);
}

int dhash_process_query_reply_success(DHash *dhash, Server *srv, uchar *data,
									  int n, Node *from)
{
	printf("dhash_process_query_reply_success\n");

	printf("starting file transfer to [%s]:%d\n", v6addr_to_str(&from->addr),
		   from->port);
}

int dhash_process_query(DHash *dhash, Server *srv, uchar *data, int n,
						Node *from)
{
	printf("dhash_process_query\n");

	uchar query_type;
	in6_addr reply_addr;
	ushort reply_port;
	ushort file_len;

	int data_len = unpack(data, "c6ss", &query_type, &reply_addr, &reply_port,
						  &file_len);
	if (data_len + file_len != n)
		weprintf("bad packet length");

	/* if the query comes from one of our servers, which can happen when we
	   write over the tunnel socket, forward it along */
	/*int i;
	for (i = 0; i < dhash->nservers; i++) {
		if (v6_addr_equals(&reply_addr, &dhash->servers[i]->node.addr))
			return 0;
	}*/

	char file[file_len+1];
	memcpy(file, data+data_len, file_len);
	file[file_len] = '\0';

	printf("received query from [%s]:%d for %s\n", v6addr_to_str(&reply_addr),
		   reply_port, file);

	/* if we have the file, notify the requesting node */
	if (dhash_local_file_exists(dhash, file)) {
		printf("we have %s\n", file);
		dhash_send_file_query_reply(srv, &reply_addr, reply_port,
									DHASH_QUERY_REPLY_SUCCESS, file);
	}
	else {
		printf("we don't have %s\n", file);
		chordID id;
		get_data_id(&id, (const uchar *)file, strlen(file));

		/* if we should have the file, as its successor, but don't, also notify
		   the requesting node */
		if (chord_is_local(srv, &id)) {
			printf("but we should, so we're replying\n", file);
			dhash_send_file_query_reply(srv, &reply_addr, reply_port,
										DHASH_QUERY_REPLY_FAILURE, file);
			printf("and listening on port %d\n", srv->node.port);
		}
		/* otherwise, forward the request to the closest finger */
		else {
			printf("so we're forwarding the query\n", file);
			return 0;
		}
	}

	printf("and we're dropping the routing packet\n");
	return 1;
}

int dhash_handle_control_packet(DHash *dhash, int sock)
{
	fprintf(stderr, "handle_control_packet\n");

	uchar buf[1024];
	int n;
	char file[1024];
	short size;

	if ((n = read(sock, buf, 1024)) < 0)
		perror("reading control packet");

	int len = unpack(buf, "s", &size);
	if (n != len + size) {
		fprintf(stderr, "handle_control_packet: packet size error\n");
		return 0;
	}
	memcpy(file, buf + len, size);
	file[size] = '\0';

	if (dhash_local_file_exists(dhash, file))
		dhash_send_control_packet(dhash, DHASH_REPLY_LOCAL, file);
	else
		dhash_send_file_query(dhash, file);

	return 0;
}

int dhash_handle_chord_packet(DHash *dhash, int sock)
{
	printf("received chord packet\n");

	uchar buf[1024];
	int n;
	if ((n = read(sock, buf, 1024)) < 0)
		perror("reading chord packet");
}

int dhash_handle_route(DHash *dhash, Server *srv, int n, uchar *buf, Node *from)
{
	printf("dhash_handle_route\n");

	uchar type;
	byte ttl;
	int len;
	ushort pkt_len;

	chordID id;
	unpack(buf, "*c*cx", &id);
	printf("received routing packet to id ");
	print_chordID(&id);
	printf("\n");

	len = unpack(buf, "cc*xs", &type, &ttl, &pkt_len);
	if (len < 0 || len + pkt_len != n)
		return CHORD_PROTOCOL_ERROR;
	assert(type == CHORD_ROUTE || type == CHORD_ROUTE_LAST);

	uchar *data = buf+len;

	switch (data[0]) {
	case DHASH_QUERY:
		return dhash_process_query(dhash, srv, data, pkt_len, from);
	case DHASH_QUERY_REPLY_SUCCESS:
		return dhash_process_query_reply_success(dhash, srv, data, pkt_len,
												 from);
	case DHASH_QUERY_REPLY_FAILURE:
		printf("query_reply_failure\n");
		return 1;
	}

	return 0;
}

int dhash_start(DHash *dhash, char **conf_files, int nservers)
{
	int dhash_tunnel[2];
	if (socketpair(AF_UNIX, SOCK_DGRAM, 0, dhash_tunnel) < 0)
		eprintf("socket_pair failed:");

	dhash->control_sock = dhash_tunnel[1];

	if (fork())
		return dhash_tunnel[0];

	setprogname("dhash");
	srandom(getpid() ^ time(0));

	UDT::startup();

	init_global_eventqueue();

	dhash->servers = (Server **)malloc(sizeof(Server *)*nservers);
	dhash->chord_tunnel_socks = (int *)malloc(sizeof(int)*nservers);
	dhash->nservers = nservers;

	int i;
	for (i = 0; i < nservers; i++) {
		/*int chord_tunnel[2];
		if (socketpair(AF_UNIX, SOCK_DGRAM, 0, chord_tunnel) < 0)
			eprintf("socket_pair failed:");*/

		dhash->servers[i] = new_server(conf_files[i], 0 /*chord_tunnel[1]*/);
		dhash->chord_tunnel_socks[i] = 0 /*chord_tunnel[0]*/;

		Server *srv = dhash->servers[i];
		eventqueue_listen_socket(srv->sock, dhash,
								 (socket_func)dhash_handle_udt_packet);

		chord_set_packet_handler(srv, CHORD_ROUTE,
								 (chord_packet_handler)dhash_handle_route);
		chord_set_packet_handler(srv, CHORD_ROUTE_LAST,
								 (chord_packet_handler)dhash_handle_route);
		chord_set_packet_handler_ctx(srv, dhash);

		//eventqueue_listen_socket(chord_tunnel[0], dhash,
		//						 (socket_func)dhash_handle_chord_packet);
	}

	eventqueue_listen_socket(dhash_tunnel[1], dhash,
							 (socket_func)dhash_handle_control_packet);

	eventqueue_loop();
}

void dhash_client_request_file(int sock, const char *file)
{
	short size = strlen(file);
	uchar buf[sizeof_fmt("s") + size];

	int n = pack(buf, "s", size);
	memcpy(buf + n, file, size);
	n += size;

	if (write(sock, buf, n) < 0)
		perror("writing file request");
}

void dhash_client_process_request_reply(int sock, void *ctx,
										dhash_request_reply_handler handler)
{
	uchar buf[1024];
	char *file;
	char code;
	short size;
	int n;

	if ((n = read(sock, buf, 1024)) < 0)
		perror("reading file request reply");

	int len = unpack(buf, "cs", &code, &size);
	if (n != len + size) {
		fprintf(stderr, "process_request_reply: packet size error\n");
		return;
	}
	memcpy(file, buf + len, size);
	file[size] = '\0';

	handler(ctx, code, file);
}
