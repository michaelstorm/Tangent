#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "chord.h"
#include "dhash.h"
#include "dispatcher.h"
#include "d_messages.pb-c.h"
#include "pack.h"
#include "process.h"
#include "send.h"

uchar msg_buf[BUFSIZE];

void dhash_unpack_control_packet(evutil_socket_t sock, short what, void *arg)
{
	fprintf(stderr, "handle_control_packet\n");

	DHash *dhash = (DHash *)arg;
	uchar buf[1024];
	int n;

	if ((n = read(sock, buf, 1024)) < 0)
		perror("reading control packet");

	dispatch_packet(dhash->control_dispatcher, buf, n, NULL);
}

int dhash_unpack_query(DHash *dhash, Server *srv, uchar *data, int n,
					   Node *from)
{
	uchar query_type;
	in6_addr reply_addr;
	ushort reply_port;
	ushort name_len;

	int data_len = unpack(data, "c6ss", &query_type, &reply_addr, &reply_port,
						  &name_len);

	assert(query_type == DHASH_QUERY);
	if (data_len + name_len != n)
		weprintf("bad packet length");

	uchar *name = data+data_len;
	return dhash_process_query(dhash, srv, &reply_addr, reply_port, name,
							   name_len, from);
}

int dhash_unpack_query_reply_success(DHash *dhash, Server *srv, uchar *data,
									  int n, Node *from)
{
	uchar code;
	ushort name_len;

	int data_len = unpack(data, "cs", &code, &name_len);
	assert(code == DHASH_QUERY_REPLY_SUCCESS);

	uchar *name = data+data_len;
	return dhash_process_query_reply_success(dhash, srv, name, name_len, from);
}

int dhash_unpack_query_reply_failure(DHash *dhash, Server *srv, uchar *data,
									 int n, Node *from)
{
	uchar code;
	ushort name_len;

	int data_len = unpack(data, "cs", &code, &name_len);
	assert(code == DHASH_QUERY_REPLY_FAILURE);

	uchar *name = data+data_len;
	return dhash_process_query_reply_failure(dhash, srv, name, name_len, from);
}

static int (*chord_unpack_fn[])(DHash *, Server *, uchar *, int, Node *) = {
	dhash_unpack_query,
	dhash_unpack_query_reply_success,
	dhash_unpack_query_reply_failure,
	dhash_unpack_push,
	dhash_unpack_push_reply
};

int dhash_unpack_chord_data(Header *header, DHashPacketArgs *args, Data *msg,
							Node *from)
{
	Server *srv = args->chord_args.srv;
	DHash *dhash = args->dhash;

	fprintf(stderr, "received routing packet\n");
	uchar dhash_type = msg->data.data[0];
	if (dhash_type < NELEMS(chord_unpack_fn)) {
		int ret = chord_unpack_fn[dhash_type](dhash, srv, msg->data.data,
											  msg->data.len, from);

		// remove this, should use dispatcher and each dhash_process_* should
		// call process_data individually
		if (!ret)
			process_data(header, (ChordPacketArgs *)args, msg, from);
	}
	else
		fprintf(stderr, "unknown packet type %02x\n", dhash_type);

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
	int n = pack(buf, "c6ss", DHASH_QUERY, addr, port, name_len);
	memcpy(buf + n, name, name_len);
	return n + name_len;
}

int dhash_pack_query_reply_success(uchar *buf, const uchar *name,
								   int name_len)
{
	int n = pack(buf, "cs", DHASH_QUERY_REPLY_SUCCESS, name_len);
	memcpy(buf + n, name, name_len);
	return n + name_len;
}

int dhash_pack_query_reply_failure(uchar *buf, const uchar *name, int name_len)
{
	int n = pack(buf, "cs", DHASH_QUERY_REPLY_FAILURE, name_len);
	memcpy(buf + n, name, name_len);
	return n + name_len;
}

int dhash_pack_push(uchar *buf, in6_addr *addr, ushort port, const uchar *name,
					int name_len)
{
	int n = pack(buf, "c6ss", DHASH_PUSH, addr, port, name_len);
	memcpy(buf + n, name, name_len);
	return n + name_len;
}

int dhash_unpack_push(DHash *dhash, Server *srv, uchar *data, int n, Node *from)
{
	uchar query_type;
	in6_addr reply_addr;
	ushort reply_port;
	ushort name_len;

	int data_len = unpack(data, "c6ss", &query_type, &reply_addr, &reply_port,
						  &name_len);

	assert(query_type == DHASH_PUSH);
	if (data_len + name_len != n)
		weprintf("bad packet length");

	uchar *name = data+data_len;
	dhash_process_push(dhash, srv, &reply_addr, reply_port, name, name_len,
					   from);
	return 1;
}

int dhash_pack_push_reply(uchar *buf, const uchar *name, int name_len)
{
	int n = pack(buf, "cs", DHASH_PUSH_REPLY, name_len);
	memcpy(buf + n, name, name_len);
	return n + name_len;
}

int dhash_unpack_push_reply(DHash *dhash, Server *srv, uchar *data, int n,
							Node *from)
{
	uchar code;
	ushort name_len;

	int data_len = unpack(data, "cs", &code, &name_len);
	assert(code == DHASH_PUSH_REPLY);

	uchar *name = data+data_len;
	return dhash_process_push_reply(dhash, srv, name, name_len, from);
}

int dhash_pack_client_request(uchar *buf, const uchar *name, int name_len)
{
	ClientRequest msg = CLIENT_REQUEST__INIT;
	msg.name.len = name_len;
	msg.name.data = (uint8_t *)name;
	client_request__pack(&msg, msg_buf);
	return pack_header(buf, DHASH_CLIENT_REQUEST, msg_buf,
					   client_request__get_packed_size(&msg));
}
