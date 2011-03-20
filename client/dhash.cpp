#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <udt>
#include <unistd.h>
#include <event2/event.h>
#include "chord.h"
#include "dhash.h"
#include "pack.h"
#include "send.h"
#include "transfer.h"

void dhash_add_transfer(DHash *dhash, Transfer *trans)
{
	Transfer *old_head = dhash->trans_head;
	dhash->trans_head = trans;
	trans->next = old_head;
}

void dhash_remove_transfer(DHash *dhash, Transfer *remove)
{
	if (dhash->trans_head == remove)
		dhash->trans_head = dhash->trans_head->next;
	else {
		Transfer *trans = dhash->trans_head;
		while (trans->next) {
			if (trans->next == remove) {
				trans->next = remove->next;
				break;
			}
			trans = trans->next;
		}
	}
}

void dhash_handle_udt_packet(evutil_socket_t sock, short what, void *arg)
{
	ushort type;
	if (recv(sock, &type, 2, MSG_PEEK) == 2 && type == 0xFFFF)
		return;

	DHash *dhash = (DHash *)arg;
	Transfer *trans;
	for (trans = dhash->trans_head; trans != NULL; trans = trans->next) {
		if (trans->state == TRANSFER_RECEIVING
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

void handle_transfer_statechange(Transfer *trans, int old_state, void *arg)
{
	DHash *dhash = (DHash *)arg;
	if (trans->state == TRANSFER_COMPLETE || trans->state == TRANSFER_FAILED) {
		dhash_send_control_transfer_complete(dhash, trans);
		dhash_remove_transfer(dhash, trans);
		free_transfer(trans);
	}
}

int dhash_process_query_reply_success(DHash *dhash, Server *srv, uchar *data,
									  int n, Node *from)
{
	fprintf(stderr, "dhash_process_query_reply_success\n");

	uchar code;
	ushort name_len;
	int file_size;

	int data_len = unpack(data, "cls", &code, &file_size, &name_len);
	assert(code == DHASH_QUERY_REPLY_SUCCESS);

	char file[name_len+1];
	memcpy(file, data + data_len, name_len);
	file[name_len] = '\0';

	fprintf(stderr, "receiving transfer of \"%s\" of size %d from [%s]:%d\n", file,
		   file_size, v6addr_to_str(&from->addr), from->port);

	Transfer *trans = new_transfer(dhash->ev_base, file, srv->sock, &from->addr,
								   from->port);
	transfer_set_statechange_cb(trans, handle_transfer_statechange, dhash);

	dhash_add_transfer(dhash, trans);
	transfer_start_receiving(trans, dhash->files_path, file_size);
}

int dhash_process_query(DHash *dhash, Server *srv, uchar *data, int n,
						Node *from)
{
	fprintf(stderr, "dhash_process_query\n");

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

	fprintf(stderr, "received query from [%s]:%d for %s\n", v6addr_to_str(&reply_addr),
		   reply_port, file);

	/* if we have the file, notify the requesting node */
	if (dhash_local_file_exists(dhash, file)) {
		fprintf(stderr, "we have %s\n", file);
		dhash_send_query_reply_success(dhash, srv, &reply_addr, reply_port,
									   file);

		Transfer *trans = new_transfer(dhash->ev_base, file, srv->sock,
									   &reply_addr, reply_port);

		dhash_add_transfer(dhash, trans);

		transfer_start_sending(trans, dhash->files_path);
	}
	else {
		fprintf(stderr, "we don't have %s\n", file);
		chordID id;
		get_data_id(&id, (const uchar *)file, strlen(file));

		/* if we should have the file, as its successor, but don't, also notify
		   the requesting node */
		if (chord_is_local(srv, &id)) {
			fprintf(stderr, "but we should, so we're replying\n", file);
			dhash_send_query_reply_failure(dhash, srv, &reply_addr, reply_port,
										   file);
			fprintf(stderr, "and listening on port %d\n", srv->node.port);
		}
		/* otherwise, forward the request to the closest finger */
		else {
			fprintf(stderr, "so we're forwarding the query\n", file);
			return 0;
		}
	}

	fprintf(stderr, "and we're dropping the routing packet\n");
	return 1;
}

void dhash_process_client_query(DHash *dhash, const char *file)
{
	if (dhash_local_file_exists(dhash, file))
		dhash_send_control_packet(dhash, DHASH_CLIENT_REPLY_LOCAL, file);
	else
		dhash_send_file_query(dhash, file);
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

	dhash->ev_base = event_base_new();

	dhash->servers = (Server **)malloc(sizeof(Server *)*nservers);
	dhash->chord_tunnel_socks = (int *)malloc(sizeof(int)*nservers);
	dhash->nservers = nservers;

	int i;
	for (i = 0; i < nservers; i++) {
		/*int chord_tunnel[2];
		if (socketpair(AF_UNIX, SOCK_DGRAM, 0, chord_tunnel) < 0)
			eprintf("socket_pair failed:");*/

		dhash->servers[i] = new_server(dhash->ev_base, 0 /*chord_tunnel[1]*/);
		dhash->chord_tunnel_socks[i] = 0 /*chord_tunnel[0]*/;

		Server *srv = dhash->servers[i];
		server_initialize_from_file(srv, conf_files[i]);
		server_initialize_socket(srv);

		dhash->udt_sock_event = event_new(dhash->ev_base, srv->sock,
										  EV_READ|EV_PERSIST,
										  dhash_handle_udt_packet, dhash);
		event_add(dhash->udt_sock_event, NULL);

		chord_set_packet_handler(srv, CHORD_ROUTE,
								 (chord_packet_handler)dhash_unpack_chord_packet);
		chord_set_packet_handler(srv, CHORD_ROUTE_LAST,
								 (chord_packet_handler)dhash_unpack_chord_packet);
		chord_set_packet_handler_ctx(srv, dhash);

		server_start(srv);
	}

	dhash->control_sock_event = event_new(dhash->ev_base, dhash->control_sock,
										  EV_READ|EV_PERSIST,
										  dhash_unpack_control_packet, dhash);
	event_add(dhash->control_sock_event, NULL);

	event_base_dispatch(dhash->ev_base);
}

void dhash_client_request_file(int sock, const char *file)
{
	short size = strlen(file);
	uchar buf[sizeof_fmt("s") + size];

	int n = pack(buf, "cs", DHASH_CLIENT_QUERY, size);
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
