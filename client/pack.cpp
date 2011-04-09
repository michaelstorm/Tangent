#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "chord.h"
#include "dhash.h"
#include "d_messages.pb-c.h"
#include "pack.h"
#include "process.h"
#include "send.h"

void dhash_unpack_control_packet(evutil_socket_t sock, short what, void *arg)
{
	fprintf(stderr, "handle_control_packet\n");

	DHash *dhash = (DHash *)arg;
	uchar buf[1024];
	int n;

	if ((n = read(sock, buf, 1024)) < 0)
		perror("reading control packet");

	uchar type = buf[0];
	switch (type) {
	case DHASH_CLIENT_QUERY:
	{
		ClientRequest *msg = client_request__unpack(NULL, n-1, buf+1);
		dhash_process_client_request(dhash, msg);
		break;
	}
	}

	return;
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

	char file[name_len+1];
	memcpy(file, data+data_len, name_len);
	file[name_len] = '\0';

	return dhash_process_query(dhash, srv, &reply_addr, reply_port, file, from);
}

int dhash_unpack_query_reply_success(DHash *dhash, Server *srv, uchar *data,
									  int n, Node *from)
{
	uchar code;
	ushort name_len;

	int data_len = unpack(data, "cs", &code, &name_len);
	assert(code == DHASH_QUERY_REPLY_SUCCESS);

	char file[name_len+1];
	memcpy(file, data + data_len, name_len);
	file[name_len] = '\0';

	return dhash_process_query_reply_success(dhash, srv, file, from);
}

int dhash_unpack_query_reply_failure(DHash *dhash, Server *srv, uchar *data,
									 int n, Node *from)
{
	uchar code;
	ushort name_len;

	int data_len = unpack(data, "cs", &code, &name_len);
	assert(code == DHASH_QUERY_REPLY_FAILURE);

	char file[name_len+1];
	memcpy(file, data + data_len, name_len);
	file[name_len] = '\0';

	return dhash_process_query_reply_failure(dhash, srv, file, from);
}

static int (*chord_unpack_fn[])(DHash *, Server *, uchar *, int, Node *) = {
	dhash_unpack_query,
	dhash_unpack_query_reply_success,
	dhash_unpack_query_reply_failure,
	dhash_unpack_push,
	dhash_unpack_push_reply
};

static int dhash_unpack_chord_packet(Header *header, DHashPacketArgs *args,
									 int type, Data *msg, Node *from)
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
			process_data(header, (ChordPacketArgs *)args, type, msg, from);
	}
	else
		fprintf(stderr, "unknown packet type %02x\n", dhash_type);

	return 0;
}

int dhash_unpack_chord_route(Header *header, DHashPacketArgs *args, Data *msg,
							 Node *from)
{
	return dhash_unpack_chord_packet(header, args, CHORD_ROUTE, msg, from);
}

int dhash_unpack_chord_route_last(Header *header, DHashPacketArgs *args,
								  Data *msg, Node *from)
{
	return dhash_unpack_chord_packet(header, args, CHORD_ROUTE_LAST, msg, from);
}

/* Clients are likely to have different event loops and not particularly
   care about packet internals. So this function can be called when the control
   socket is readable, at which point it unpacks a reply from the DHash server
   and calls the handler with the response data.
 */
int dhash_client_unpack_request_reply(int sock, void *ctx,
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
		return 1;
	}

	memcpy(file, buf + len, size);
	file[size] = '\0';

	handler(ctx, code, file);
	return 0;
}

int dhash_pack_control_request_reply(uchar *buf, int code, const char *name,
									 int name_len)
{
	int n = pack(buf, "cs", code, name_len);
	memcpy(buf + n, name, name_len);
	return n + name_len;
}

int dhash_pack_query(uchar *buf, in6_addr *addr, ushort port, const char *name,
					 int name_len)
{
	int n = pack(buf, "c6ss", DHASH_QUERY, addr, port, name_len);
	memcpy(buf + n, name, name_len);
	return n + name_len;
}

int dhash_pack_query_reply_success(uchar *buf, const char *name,
								   int name_len)
{
	int n = pack(buf, "cs", DHASH_QUERY_REPLY_SUCCESS, name_len);
	memcpy(buf + n, name, name_len);
	return n + name_len;
}

int dhash_pack_query_reply_failure(uchar *buf, const char *name, int name_len)
{
	int n = pack(buf, "cs", DHASH_QUERY_REPLY_FAILURE, name_len);
	memcpy(buf + n, name, name_len);
	return n + name_len;
}

int dhash_pack_push(uchar *buf, in6_addr *addr, ushort port, const char *name,
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

	char file[name_len+1];
	memcpy(file, data+data_len, name_len);
	file[name_len] = '\0';

	dhash_process_push(dhash, srv, &reply_addr, reply_port, file, from);
	return 1;
}

int dhash_pack_push_reply(uchar *buf, const char *name, int name_len)
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

	char file[name_len+1];
	memcpy(file, data + data_len, name_len);
	file[name_len] = '\0';

	return dhash_process_push_reply(dhash, srv, file, from);
}

int dhash_pack_client_request(uchar *buf, const char *name)
{
	ClientRequest msg = CLIENT_REQUEST__INIT;
	msg.name = (char *)name;
	client_request__pack(&msg, buf+1);
	buf[0] = DHASH_CLIENT_QUERY;
	return client_request__get_packed_size(&msg)+1;
}
