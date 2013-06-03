#include <assert.h>
#include <string.h>
#include <openssl/cms.h>
#include <openssl/err.h>
#include "chord/chord.h"
#include "chord/messages.pb-c.h"
#include "chord/process.h"
#include "chord/util.h"
#include "dhash.h"
#include "process.h"
#include "send.h"
#include "transfer.h"

int dhash_process_query(Header *header, ChordDataPacketArgs *args, Query *msg,
						Node *from)
{
	DHash *dhash = args->dhash;
	ChordServer *srv = args->srv;

	in6_addr reply_addr;
	v6_addr_set(&reply_addr, msg->reply_addr.data);

	if (msg->name.len == 0) {
		fprintf(stderr, "dropping query for zero-length name from [%s]:%d ",
				v6addr_to_str(&from->addr), from->port);
		fprintf(stderr, "(reply addr [%s]:%d)\n", v6addr_to_str(&reply_addr),
				msg->reply_port);
		return 0;
	}

	fprintf(stderr, "received query from [%s]:%d for %s\n",
			v6addr_to_str(&reply_addr), msg->reply_port,
			buf_to_str(msg->name.data, msg->name.len));

	/* if we have the file, notify the requesting node */
	if (dhash_local_file_exists(dhash, msg->name.data, msg->name.len)) {
		fprintf(stderr, "we have %s\n", buf_to_str(msg->name.data,
												   msg->name.len));
		dhash_send_query_reply_success(dhash, srv, &reply_addr,
									   msg->reply_port, msg->name.data,
									   msg->name.len);

		Transfer *trans = new_transfer(srv->node.port+1, &reply_addr,
									   msg->reply_port+1, dhash->files_path);
		transfer_start_sending(trans);
	}
	else {
		fprintf(stderr, "we don't have %s\n", buf_to_str(msg->name.data,
														 msg->name.len));
		chordID id;
		get_data_id(&id, msg->name.data, msg->name.len);

		/* if we should have the file, as its successor, but don't, also notify
		   the requesting node */
		if (chord_is_local(srv, &id)) {
			fprintf(stderr, "but we should, so we're replying\n");
			dhash_send_query_reply_failure(dhash, srv, &reply_addr,
										   msg->reply_port, msg->name.data,
										   msg->name.len);
			fprintf(stderr, "and listening on port %d\n", srv->node.port);
		}
		/* otherwise, forward the request to the closest finger */
		else {
			fprintf(stderr, "so we're forwarding the query\n");
			return 1;
		}
	}

	fprintf(stderr, "and we're dropping the routing packet\n");
	return 0;
}

static void receive_success(Transfer *trans, void *arg)
{
	DHash *dhash = (DHash *)arg;

	char abs_file_path[1024];
	sprintf(abs_file_path, "%s/%s", dhash->files_path, trans->file);

	BIO *bio = BIO_new_file(abs_file_path, "rb");
	if (!bio) {
		fprintf(stderr, "error: could not open file \"%s\" for verification\n",
				abs_file_path);
		return;
	}

	CMS_ContentInfo *cms = d2i_CMS_bio(bio, NULL);
	if (!cms) {
		fprintf(stderr, "error parsing CMS structure in \"%s\": %s\n",
				abs_file_path, ERR_error_string(ERR_get_error(), NULL));
		return;
	}

	if (!CMS_verify(cms, dhash->cert_stack, NULL, NULL, NULL, CMS_NOINTERN)) {
		fprintf(stderr, "error: signature in \"%s\" is invalid\n",
				abs_file_path);
		return;
	}

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

int dhash_process_query_reply_success(Header *header, ChordDataPacketArgs *args,
									  QueryReplySuccess *msg, Node *from)
{
	DHash *dhash = args->dhash;
	ChordServer *srv = args->srv;

	fprintf(stderr, "receiving transfer of \"%s\" from [%s]:%d\n",
			buf_to_str(msg->name.data, msg->name.len),
			v6addr_to_str(&from->addr), from->port);

	Transfer *trans = new_transfer(srv->node.port+1, &from->addr, from->port+1,
								   dhash->files_path);
	transfer_set_callbacks(trans, receive_success, receive_fail, dhash,
						   dhash->ev_base);
	transfer_start_receiving(trans, buf_to_str(msg->name.data, msg->name.len));
	return 0;
}

int dhash_process_query_reply_failure(Header *header, ChordDataPacketArgs *args,
									  QueryReplyFailure *msg, Node *from)
{
	DHash *dhash = args->dhash;
	dhash_send_control_query_failure(dhash, msg->name.data, msg->name.len);
	return 0;
}

int dhash_process_push(Header *header, ChordDataPacketArgs *args, Push *msg,
					   Node *from)
{
	DHash *dhash = args->dhash;
	ChordServer *srv = args->srv;

	in6_addr reply_addr;
	v6_addr_set(&reply_addr, msg->reply_addr.data);

	fprintf(stderr, "received push for \"%s\"\n", buf_to_str(msg->name.data,
															 msg->name.len));
	if (dhash_local_file_exists(dhash, msg->name.data, msg->name.len)) {
		fprintf(stderr, "but we already have the file, so not replying\n");
		return 0;
	}
	fprintf(stderr, "and sending push reply\n");

	dhash_send_push_reply(dhash, srv, &reply_addr, msg->reply_port,
						  msg->name.data, msg->name.len);

	Transfer *trans = new_transfer(srv->node.port+1, &reply_addr,
								   msg->reply_port+1, dhash->files_path);
	transfer_start_receiving(trans, buf_to_str(msg->name.data, msg->name.len));
	return 0;
}

int dhash_process_push_reply(Header *header, ChordDataPacketArgs *args,
							 PushReply *msg, Node *from)
{
	DHash *dhash = args->dhash;
	ChordServer *srv = args->srv;

	fprintf(stderr, "received push reply for \"%s\"\n",
			buf_to_str(msg->name.data, msg->name.len));

	Transfer *trans = new_transfer(srv->node.port+1, &from->addr, from->port+1,
								   dhash->files_path);
	transfer_start_sending(trans);
	return 0;
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
