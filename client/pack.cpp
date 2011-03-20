#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "chord.h"
#include "dhash.h"
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

int dhash_unpack_chord_packet(DHash *dhash, Server *srv, int n, uchar *buf,
							  Node *from)
{
	uchar type;
	byte ttl;
	int len;
	ushort pkt_len;

	chordID id;
	unpack(buf, "*c*cx", &id);
	fprintf(stderr, "received routing packet to id ");
	print_chordID(&id);
	fprintf(stderr, "\n");

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
		fprintf(stderr, "query_reply_failure\n");
		return 1;
	}

	return 0;
}
