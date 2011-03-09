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

Transfer *new_transfer(char *file, int chord_sock, const in6_addr *addr,
					   ushort port)
{
	Transfer *trans = (Transfer *)malloc(sizeof(Transfer));
	trans->chord_sock = chord_sock;
	trans->next = NULL;
	trans->file = (char *)malloc(strlen(file)+1);
	strcpy(trans->file, file);

	trans->received = 0;
	trans->size = 0;

	trans->state = DHASH_TRANSFER_IDLE;

	trans->udt_sock = UDT::socket(V4_MAPPED(addr) ? AF_INET : AF_INET6,
								  SOCK_STREAM, 0);

	int yes = true;
	UDT::setsockopt(trans->udt_sock, 0, UDT_RENDEZVOUS, &yes, sizeof(yes));
	UDT::setsockopt(trans->udt_sock, 0, UDT_SNDSYN, &yes, sizeof(yes));
	UDT::setsockopt(trans->udt_sock, 0, UDT_RCVSYN, &yes, sizeof(yes));

	if (UDT::ERROR == UDT::bind(trans->udt_sock, chord_sock)) {
		fprintf(stderr, "bind error: %s\n",
				UDT::getlasterror().getErrorMessage());
		return NULL;
	}

	int err;
	if (V4_MAPPED(addr)) {
		sockaddr_in serv_addr;
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(port);
		serv_addr.sin_addr.s_addr = to_v4addr(addr);
		memset(&(serv_addr.sin_zero), '\0', 8);

		err = UDT::connect(trans->udt_sock, (sockaddr*)&serv_addr,
						   sizeof(serv_addr));
	}
	else {
		sockaddr_in6 serv_addr;
		serv_addr.sin6_family = AF_INET6;
		serv_addr.sin6_port = htons(port);
		v6_addr_copy(&serv_addr.sin6_addr, addr);

		err = UDT::connect(trans->udt_sock, (sockaddr*)&serv_addr,
						   sizeof(serv_addr));
	}

	if (err == UDT::ERROR) {
		fprintf(stderr, "connect: %s\n", UDT::getlasterror().getErrorMessage());
		return NULL;
	}

	return trans;
}

int transfer_receive(Transfer *trans, int sock)
{
	assert(trans->state == DHASH_TRANSFER_RECEIVING);

	uchar buf[1024];
	int len = UDT::recv(trans->udt_sock, (char *)buf, sizeof(buf), 0);

	printf("received %ld/%ld of \"%s\"\n", trans->received, trans->size,
		   trans->file);

	if (fwrite(buf, 1, len, trans->fp) < len) {
		weprintf("writing to \"%\":", trans->file);
		return 1;
	}

	trans->received += len;
	if (trans->received == trans->size) {
		printf("done receiving \"%s\"\n", trans->file);
		fclose(trans->fp);
		return 1;
	}
	else if (trans->received >= trans->size) {
		printf("received size %ld greater than expected size %ld for \"%s\"\n",
			   trans->received, trans->size, trans->file);
		fclose(trans->fp);
		return 1;
	}

	return 0;
}

int transfer_send(Transfer *trans, int sock)
{
	assert(trans->state == DHASH_TRANSFER_SENDING);

	uchar buf[1024];
	int n = fread(buf, 1, sizeof(buf), trans->fp);
	printf("read %d bytes from %s\n", n, trans->file);

	if (n < 0) {
		weprintf("reading from \"%s\":", trans->file);
		fclose(trans->fp);
		return 1;
	}
	else if (n == 0) {
		printf("done reading from \"%s\"\n", trans->file);
		fclose(trans->fp);
		return 1;
	}

	if (UDT::ERROR == UDT::send(trans->udt_sock, (char *)buf, n, 0)) {
		fprintf(stderr, "send: %s\n", UDT::getlasterror().getErrorMessage());
		return 1;
	}

	return 0;
}

void transfer_start_receiving(Transfer *trans, const char *dir, int size)
{
	assert(trans->state == DHASH_TRANSFER_IDLE);

	char path[1024];
	strcpy(path, dir);
	strcat(path, "/");
	strcat(path, trans->file);

	trans->size = size;

	if (NULL == (trans->fp = fopen(path, "wb"))) {
		weprintf("could not open \"%s\" for reading:", path);
		return;
	}

	trans->state = DHASH_TRANSFER_RECEIVING;
	printf("started receiving %s\n", path);
}

void transfer_start_sending(Transfer *trans, const char *dir)
{
	assert(trans->state == DHASH_TRANSFER_IDLE);

	char path[1024];
	strcpy(path, dir);
	strcat(path, "/");
	strcat(path, trans->file);

	if (NULL == (trans->fp = fopen(path, "rb"))) {
		weprintf("could not open \"%s\" for reading:", path);
		return;
	}

	trans->state = DHASH_TRANSFER_SENDING;
	printf("started sending %s\n", path);
	eventqueue_listen_socket(trans->chord_sock, trans,
							 (socket_func)transfer_send, SOCKET_WRITE);
}

void dhash_add_transfer(DHash *dhash, Transfer *trans)
{
	Transfer *old_head = dhash->trans_head;
	dhash->trans_head = trans;
	trans->next = old_head;
}

int dhash_handle_udt_packet(DHash *dhash, int sock)
{
	ushort type;
	if (recv(sock, &type, 2, MSG_PEEK) == 2 && type == 0xFFFF)
		return 0;

	Transfer *trans;
	for (trans = dhash->trans_head; trans != NULL; trans = trans->next) {
		if (trans->state == DHASH_TRANSFER_RECEIVING
			&& trans->chord_sock == sock)
			transfer_receive(trans, sock);
	}
}

DHash *new_dhash(const char *files_path)
{
	DHash *dhash = (DHash *)malloc(sizeof(DHash));
	dhash->servers = NULL;
	dhash->nservers = 0;
	dhash->trans_head = NULL;

	dhash->files_path = (char *)malloc(strlen(files_path)+1);
	strcpy(dhash->files_path, files_path);
	return dhash;
}

int dhash_stat_local_file(DHash *dhash, const char *file, struct stat *stat_buf)
{
	char abs_file_path[strlen(dhash->files_path) + strlen(file) + 1];

	strcpy(abs_file_path, dhash->files_path);
	strcpy(abs_file_path + strlen(dhash->files_path), "/");
	strcpy(abs_file_path + strlen(dhash->files_path)+1, file);

	return stat(abs_file_path, stat_buf);
}

int dhash_local_file_exists(DHash *dhash, const char *file)
{
	struct stat stat_buf;
	return dhash_stat_local_file(dhash, file, &stat_buf) == 0;
}

int dhash_local_file_size(DHash *dhash, const char *file)
{
	struct stat stat_buf;
	assert(dhash_stat_local_file(dhash, file, &stat_buf) == 0);
	return stat_buf.st_size;
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

int dhash_process_query_reply_success(DHash *dhash, Server *srv, uchar *data,
									  int n, Node *from)
{
	printf("dhash_process_query_reply_success\n");

	uchar code;
	ushort name_len;
	int file_size;

	int data_len = unpack(data, "cls", &code, &file_size, &name_len);
	assert(code == DHASH_QUERY_REPLY_SUCCESS);

	char file[name_len+1];
	memcpy(file, data + data_len, name_len);
	file[name_len] = '\0';

	printf("receiving transfer of \"%s\" of size %d from [%s]:%d\n", file,
		   file_size, v6addr_to_str(&from->addr), from->port);

	Transfer *trans = new_transfer(file, srv->sock, &from->addr, from->port);
	dhash_add_transfer(dhash, trans);
	transfer_start_receiving(trans, dhash->files_path, file_size);
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

	char file[file_len+1];
	memcpy(file, data+data_len, file_len);
	file[file_len] = '\0';

	printf("received query from [%s]:%d for %s\n", v6addr_to_str(&reply_addr),
		   reply_port, file);

	/* if we have the file, notify the requesting node */
	if (dhash_local_file_exists(dhash, file)) {
		printf("we have %s\n", file);
		dhash_send_query_reply_success(dhash, srv, &reply_addr, reply_port,
									   file);

		Transfer *trans = new_transfer(file, srv->sock, &reply_addr,
									   reply_port);

		dhash_add_transfer(dhash, trans);

		transfer_start_sending(trans, dhash->files_path);
	}
	else {
		printf("we don't have %s\n", file);
		chordID id;
		get_data_id(&id, (const uchar *)file, strlen(file));

		/* if we should have the file, as its successor, but don't, also notify
		   the requesting node */
		if (chord_is_local(srv, &id)) {
			printf("but we should, so we're replying\n", file);
			dhash_send_query_reply_failure(dhash, srv, &reply_addr, reply_port,
										   file);
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

		dhash->servers[i] = new_server(0 /*chord_tunnel[1]*/);
		dhash->chord_tunnel_socks[i] = 0 /*chord_tunnel[0]*/;

		Server *srv = dhash->servers[i];
		server_initialize_from_file(srv, conf_files[i]);
		server_initialize_socket(srv);

		eventqueue_listen_socket(srv->sock, dhash,
								 (socket_func)dhash_handle_udt_packet,
								 SOCKET_READ);

		chord_set_packet_handler(srv, CHORD_ROUTE,
								 (chord_packet_handler)dhash_handle_route);
		chord_set_packet_handler(srv, CHORD_ROUTE_LAST,
								 (chord_packet_handler)dhash_handle_route);
		chord_set_packet_handler_ctx(srv, dhash);

		server_start(srv);

		//eventqueue_listen_socket(chord_tunnel[0], dhash,
		//						 (socket_func)dhash_handle_chord_packet);
	}

	eventqueue_listen_socket(dhash->control_sock, dhash,
							 (socket_func)dhash_handle_control_packet,
							 SOCKET_READ);

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
	char file[128];
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
