#include <assert.h>
#include <string.h>
#include "chord.h"
#include "d_messages.pb-c.h"
#include "dhash.h"
#include "process.h"
#include "send.h"
#include "transfer.h"

int dhash_process_query(DHash *dhash, Server *srv, in6_addr *reply_addr,
						ushort reply_port, const uchar *name, int name_len,
						Node *from)
{
	if (name_len == 0) {
		fprintf(stderr, "dropping query for zero-length name from [%s]:%d ",
				v6addr_to_str(&from->addr), from->port);
		fprintf(stderr, "(reply addr [%s]:%d)\n", v6addr_to_str(reply_addr),
				reply_port);
		return 1;
	}

	fprintf(stderr, "received query from [%s]:%d for %s\n",
			v6addr_to_str(reply_addr), reply_port, buf_to_str(name, name_len));

	/* if we have the file, notify the requesting node */
	if (dhash_local_file_exists(dhash, name, name_len)) {
		fprintf(stderr, "we have %s\n", buf_to_str(name, name_len));
		dhash_send_query_reply_success(dhash, srv, reply_addr, reply_port,
									   name, name_len);

		Transfer *trans = new_transfer(srv->node.port+1, reply_addr,
									   reply_port+1, dhash->files_path);
		transfer_start_sending(trans);
	}
	else {
		fprintf(stderr, "we don't have %s\n", buf_to_str(name, name_len));
		chordID id;
		get_data_id(&id, (const uchar *)name, name_len);

		/* if we should have the file, as its successor, but don't, also notify
		   the requesting node */
		if (chord_is_local(srv, &id)) {
			fprintf(stderr, "but we should, so we're replying\n");
			dhash_send_query_reply_failure(dhash, srv, reply_addr, reply_port,
										   name, name_len);
			fprintf(stderr, "and listening on port %d\n", srv->node.port);
		}
		/* otherwise, forward the request to the closest finger */
		else {
			fprintf(stderr, "so we're forwarding the query\n");
			return 0;
		}
	}

	fprintf(stderr, "and we're dropping the routing packet\n");
	return 1;
}

static void receive_success(Transfer *trans, void *arg)
{
	DHash *dhash = (DHash *)arg;
	dhash_send_control_query_success(dhash, (uchar *)trans->file,
									 strlen(trans->file));
	dhash_send_push(dhash, (uchar *)trans->file, strlen(trans->file));
	free_transfer(trans);
}

static void receive_fail(Transfer *trans, void *arg)
{
	DHash *dhash = (DHash *)arg;
	dhash_send_control_query_failure(dhash, (uchar *)trans->file,
									 strlen(trans->file));
	free_transfer(trans);
}

int dhash_process_query_reply_success(DHash *dhash, Server *srv,
									  const uchar *name, int name_len,
									  Node *from)
{
	fprintf(stderr, "receiving transfer of \"%s\" from [%s]:%d\n",
			buf_to_str(name, name_len), v6addr_to_str(&from->addr), from->port);

	Transfer *trans = new_transfer(srv->node.port+1, &from->addr, from->port+1,
								   dhash->files_path);
	transfer_set_callbacks(trans, receive_success, receive_fail, dhash,
						   dhash->ev_base);
	transfer_start_receiving(trans, buf_to_str(name, name_len));
	return 0;
}

int dhash_process_query_reply_failure(DHash *dhash, Server *srv,
									  const uchar *name, int name_len,
									  Node *from)
{
	dhash_send_control_query_failure(dhash, name, name_len);
	return 0;
}

int dhash_process_push(DHash *dhash, Server *srv, in6_addr *reply_addr,
					   ushort reply_port, const uchar *name, int name_len,
					   Node *from)
{
	fprintf(stderr, "received push for \"%s\"\n", buf_to_str(name, name_len));
	if (dhash_local_file_exists(dhash, name, name_len)) {
		fprintf(stderr, "but we already have the file, so not replying\n");
		return 0;
	}
	fprintf(stderr, "and sending push reply\n");

	dhash_send_push_reply(dhash, srv, reply_addr, reply_port, name, name_len);

	Transfer *trans = new_transfer(srv->node.port+1, reply_addr, reply_port+1,
								   dhash->files_path);
	transfer_start_receiving(trans, buf_to_str(name, name_len));
	return 0;
}

int dhash_process_push_reply(DHash *dhash, Server *srv, const uchar *name,
							 int name_len, Node *from)
{
	fprintf(stderr, "received push reply for \"%s\"\n", buf_to_str(name,
																   name_len));

	Transfer *trans = new_transfer(srv->node.port+1, &from->addr, from->port+1,
								   dhash->files_path);
	transfer_start_sending(trans);
}

int dhash_process_client_request(Header *header, ControlPacketArgs *args,
								 ClientRequest *msg, Node *from)
{
	DHash *dhash = args->dhash;
	if (dhash_local_file_exists(dhash, msg->name.data, msg->name.len))
		dhash_send_control_packet(dhash, DHASH_CLIENT_REPLY_LOCAL,
								  msg->name.data, msg->name.len);
	else
		dhash_send_query(dhash, msg->name.data, msg->name.len);
	return 0;
}
