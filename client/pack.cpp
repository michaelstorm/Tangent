#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "chord.h"
#include "dhash.h"
#include "pack.h"
#include "process.h"
#include "send.h"

void dhash_unpack_control_packet(evutil_socket_t sock, short what, void *arg)
{
	fprintf(stderr, "handle_control_packet\n");

	DHash *dhash = (DHash *)arg;
	uchar buf[1024];
	int n;
	char file[1024];
	short size;
	uchar type;

	if ((n = read(sock, buf, 1024)) < 0)
		perror("reading control packet");

	int len = unpack(buf, "cs", &type, &size);
	if (n != len + size) {
		fprintf(stderr, "handle_control_packet: packet size error\n");
		return;
	}
	memcpy(file, buf + len, size);
	file[size] = '\0';

	switch (type) {
	case DHASH_CLIENT_QUERY:
		dhash_process_client_query(dhash, file);
		break;
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

int dhash_unpack_chord_packet(DHash *dhash, Server *srv, int n, uchar *buf,
							  Node *from)
{
	chordID id;
	unpack(buf, "*c*cx", &id);

	fprintf(stderr, "received routing packet to id ");
	print_chordID(&id);
	fprintf(stderr, "\n");

	uchar type;
	byte ttl;
	ushort pkt_len;
	int len = unpack(buf, "cc*xs", &type, &ttl, &pkt_len);

	if (len < 0 || len + pkt_len != n)
		return CHORD_PROTOCOL_ERROR;
	assert(type == CHORD_ROUTE || type == CHORD_ROUTE_LAST);

	uchar *data = buf+len;

	switch (data[0]) {
	case DHASH_QUERY:
		return dhash_unpack_query(dhash, srv, data, pkt_len, from);
	case DHASH_QUERY_REPLY_SUCCESS:
		return dhash_unpack_query_reply_success(dhash, srv, data, pkt_len,
												from);
	case DHASH_QUERY_REPLY_FAILURE:
		return dhash_unpack_query_reply_failure(dhash, srv, data, pkt_len,
												from);
	case DHASH_PUSH:
		return dhash_unpack_push(dhash, srv, data, pkt_len, from);
	}

	return 0;
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

	fprintf(stderr, "received push for \"%s\"\n", file);

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

	return 1;//dhash_process_push_reply(dhash, srv, file, from);
}
