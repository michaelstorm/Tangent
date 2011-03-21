#include <assert.h>
#include <string.h>
#include "chord.h"
#include "dhash.h"
#include "process.h"
#include "send.h"
#include "transfer.h"

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

	fprintf(stderr, "receiving transfer of \"%s\" of size %d from [%s]:%d\n",
			file, file_size, v6addr_to_str(&from->addr), from->port);

	Transfer *trans = new_transfer(dhash->ev_base, file, srv->sock, &from->addr,
								   from->port);
	transfer_set_statechange_cb(trans, dhash_handle_transfer_statechange,
								dhash);

	dhash_add_transfer(dhash, trans);
	transfer_start_receiving(trans, dhash->files_path, file_size);
}

int dhash_process_query(DHash *dhash, Server *srv, in6_addr *reply_addr,
						ushort reply_port, const char *file, Node *from)
{
	fprintf(stderr, "dhash_process_query\n");

	int file_len = strlen(file);
	if (file_len == 0) {
		fprintf(stderr, "dropping query for zero-length file from [%s]:%d ",
				v6addr_to_str(&from->addr), from->port);
		fprintf(stderr, "(reply addr [%s]:%d\n", v6addr_to_str(reply_addr),
				reply_port);
		return 1;
	}

	fprintf(stderr, "received query from [%s]:%d for %s\n",
			v6addr_to_str(reply_addr), reply_port, file);

	/* if we have the file, notify the requesting node */
	if (dhash_local_file_exists(dhash, file)) {
		fprintf(stderr, "we have %s\n", file);
		dhash_send_query_reply_success(dhash, srv, reply_addr, reply_port,
									   file);

		Transfer *trans = new_transfer(dhash->ev_base, file, srv->sock,
									   reply_addr, reply_port);

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
			dhash_send_query_reply_failure(dhash, srv, reply_addr, reply_port,
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
		dhash_send_query(dhash, file);
}
