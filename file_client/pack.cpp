#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "chord/dispatcher.h"
#include "chord/pack.h"
#include "dhash.h"
#include "d_messages.pb-c.h"
#include "pack.h"
#include "process.h"
#include "send.h"

static uchar msg_buf[BUFSIZE];

void dhash_unpack_control_packet(evutil_socket_t sock, short what, void *arg)
{
	fprintf(stderr, "handle_control_packet\n");

	DHash *dhash = (DHash *)arg;
	uchar buf[1024];
	int n;

	if ((n = read(sock, buf, 1024)) < 0)
		perror("reading control packet");

	dispatch_packet(dhash->control_dispatcher, buf, n, NULL, NULL);
}

int dhash_unpack_chord_data(Header *header, DHashPacketArgs *args, Data *msg,
							Node *from)
{
	fprintf(stderr, "received routing packet\n");
	ChordServer *srv = args->chord_args.srv;
	DHash *dhash = args->dhash;

	int type = dispatcher_get_type(msg->data.data, msg->data.len);
	dispatcher_push_arg(dhash->chord_dispatcher, type, srv);

	int process_ret;
	int ret = dispatch_packet(dhash->chord_dispatcher, msg->data.data,
							  msg->data.len, from, &process_ret);

	dispatcher_pop_arg(dhash->chord_dispatcher, type);

	if (!ret || process_ret)
		process_data(header, (ChordPacketArgs *)args, msg, from);

	return 0;
}

/* Clients are likely to have different event loops and not particularly
   care about packet internals. So this function can be called when the control
   socket is readable, at which point it unpacks a reply from the DHash server
   and calls the handler with the response data.
 */
int dhash_client_unpack_request_reply(uchar *buf, int n, void *ctx,
									  dhash_request_reply_handler handler)
{
	ClientRequestReply *reply = client_request_reply__unpack(NULL, n, buf);
	if (!reply) {
		fprintf(stderr, "error unpacking client request reply\n");
		return 1;
	}

	handler(ctx, reply->code, reply->name.data, reply->name.len);
	client_request_reply__free_unpacked(reply, NULL);
	return 0;
}

// don't use a header yet, since we've only implemented file requests
int dhash_pack_control_request_reply(uchar *buf, int code, const uchar *name,
									 int name_len)
{
	ClientRequestReply msg = CLIENT_REQUEST_REPLY__INIT;
	msg.name.len = name_len;
	msg.name.data = (uint8_t *)name;
	msg.code = code;
	return client_request_reply__pack(&msg, buf);
}

int dhash_pack_query(uchar *buf, in6_addr *addr, ushort port, const uchar *name,
					 int name_len)
{
	Query msg = QUERY__INIT;
	msg.reply_addr.len = 16;
	msg.reply_addr.data = addr->s6_addr;

	msg.reply_port = port;

	msg.name.len = name_len;
	msg.name.data = (uint8_t *)name;
	return pack_dhash_header(buf, DHASH_QUERY, &msg);
}

int dhash_pack_query_reply_success(uchar *buf, const uchar *name,
								   int name_len)
{
	QueryReplySuccess msg = QUERY_REPLY_SUCCESS__INIT;
	msg.name.len = name_len;
	msg.name.data = (uint8_t *)name;
	return pack_dhash_header(buf, DHASH_QUERY_REPLY_SUCCESS, &msg);
}

int dhash_pack_query_reply_failure(uchar *buf, const uchar *name, int name_len)
{
	QueryReplyFailure msg = QUERY_REPLY_FAILURE__INIT;
	msg.name.len = name_len;
	msg.name.data = (uint8_t *)name;
	return pack_dhash_header(buf, DHASH_QUERY_REPLY_FAILURE, &msg);
}

int dhash_pack_push(uchar *buf, in6_addr *addr, ushort port, const uchar *name,
					int name_len)
{
	Push msg = PUSH__INIT;
	msg.reply_addr.len = 16;
	msg.reply_addr.data = addr->s6_addr;

	msg.reply_port = port;

	msg.name.len = name_len;
	msg.name.data = (uint8_t *)name;
	return pack_dhash_header(buf, DHASH_PUSH, &msg);
}

int dhash_pack_push_reply(uchar *buf, const uchar *name, int name_len)
{
	PushReply msg = PUSH_REPLY__INIT;
	msg.name.len = name_len;
	msg.name.data = (uint8_t *)name;
	return pack_dhash_header(buf, DHASH_PUSH_REPLY, &msg);
}

int dhash_pack_client_request(uchar *buf, const uchar *name, int name_len)
{
	ClientRequest msg = CLIENT_REQUEST__INIT;
	msg.name.len = name_len;
	msg.name.data = (uint8_t *)name;
	return pack_dhash_header(buf, DHASH_CLIENT_REQUEST, &msg);
}
