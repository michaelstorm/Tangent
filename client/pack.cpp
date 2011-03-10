#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "chord.h"
#include "dhash.h"
#include "send.h"

int dhash_unpack_control_packet(DHash *dhash, int sock)
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
		dhash_send_control_packet(dhash, DHASH_CLIENT_REPLY_LOCAL, file);
	else
		dhash_send_file_query(dhash, file);

	return 0;
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
