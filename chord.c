/* Chord server loop */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <openssl/rand.h>
#include "chord.h"
#include "gen_utils.h"

Server *new_server(char *conf_file, int tunnel_sock)
{
	char id[4*CHORD_ID_LEN];
	int ip_ver;
	FILE *fp;

	Server *srv = calloc(1, sizeof(Server));

	srv->tunnel_sock = tunnel_sock;
	srv->to_fix_finger = NFINGERS-1;

	fp = fopen(conf_file, "r");
	if (fp == NULL)
		eprintf("fopen(%s,\"r\") failed:", conf_file);
	if (fscanf(fp, "%d\n", &ip_ver) != 1)
		eprintf("Didn't find ip version in \"%s\"", conf_file);
	if (fscanf(fp, "%hd", (short *)&srv->node.port) != 1)
		eprintf("Didn't find port in \"%s\"", conf_file);
	if (fscanf(fp, " %s\n", id) != 1)
		eprintf("Didn't find id in \"%s\"", conf_file);
//	srv->node.id = atoid(id);

	fprintf(stderr, "Chord started.\n");
	fprintf(stderr, "id="); print_id(stderr, &srv->node.id);
	fprintf(stderr, "\n");

	fprintf(stderr, "ip=%s\n", v6addr_to_str(&srv->node.addr));
	fprintf(stderr, "port=%d\n", srv->node.port);

	initialize(srv, ip_ver == 6);
	join(srv, fp);
	fclose(fp);

	eventqueue_listen_socket(srv->sock, srv, (socket_func)handle_packet);
	//eventqueue_listen_socket(srv->tunnel_sock, srv, (socket_func)handle_packet);
	discover_addr(srv);

	return srv;
}

void chord_main(char **conf_files, int nservers, int tunnel_sock)
{
	setprogname("chord");
	srandom(getpid() ^ time(0));

	init_global_eventqueue();

	Server *servers[nservers];
	int i;
	for (i = 0; i < nservers; i++)
		servers[i] = new_server(conf_files[i], tunnel_sock);

	eventqueue_loop();
}

/**********************************************************************/

void init_ticket_key(Server *srv)
{
	if (!RAND_load_file("/dev/urandom", 64)) {
		fprintf(stderr, "Could not seed random number generator.\n");
		exit(2);
	}

	uchar key_data[16];
	if (!RAND_bytes(key_data, sizeof(key_data))) {
		fprintf(stderr, "Could not generate ticket key.\n");
		exit(2);
	}

	BF_set_key(&srv->ticket_key, sizeof(key_data), key_data);
}

/* initialize: set up sockets and such <yawn> */
void initialize(Server *srv, int is_v6)
{
	setservent(1);

	if (is_v6)
		srv->sock = init_socket6(&in6addr_any, srv->node.port);
	else
		srv->sock = init_socket4(INADDR_ANY, srv->node.port);
	set_socket_nonblocking(srv->sock);

	srv->is_v6 = is_v6;

	init_ticket_key(srv);
}

/**********************************************************************/

/* handle_packet: snarf packet from network and dispatch */
int handle_packet(Server *srv, int sock)
{
	ssize_t packet_len;
	socklen_t from_len;
	Node from;
	byte buf[BUFSIZE];

	/* if this is a chord packet, the first 4 bytes should be 1111; otherwise
	   it's a UDT packet and we hand it back to be processed in this rather
	   inelegant fashion */
	if (recv(sock, buf, 1, MSG_PEEK) == 1 && (buf[0] >> 4) != 0x0F)
		return 0;

	if (srv->is_v6) {
		struct sockaddr_in6 from_sa;
		from_len = sizeof(from_sa);
		packet_len = recvfrom(sock, buf, sizeof(buf), 0,
							  (struct sockaddr *)&from_sa, &from_len);
		v6_addr_copy(&from.addr, &from_sa.sin6_addr);
		from.port = ntohs(from_sa.sin6_port);
	}
	else {
		struct sockaddr_in from_sa;
		from_len = sizeof(from_sa);
		packet_len = recvfrom(sock, buf, sizeof(buf), 0,
							  (struct sockaddr *)&from_sa, &from_len);
		to_v6addr(from_sa.sin_addr.s_addr, &from.addr);
		from.port = ntohs(from_sa.sin_port);
	}

	if (packet_len < 0) {
		if (errno != EAGAIN) {
			weprintf("recvfrom failed:"); /* ignore errors for now */
			return 0;
		}

		weprintf("handle_packet: EAGAIN");
		return 0; /* pick up this packet later */
	}

	get_address_id(&from.id, &from.addr, from.port);
	dispatch(srv, packet_len, buf, &from);
}

/**********************************************************************/

int read_keys(char *file, chordID *key_array, int max_num_keys)
{
	FILE *fp = fopen(file, "r");
	if (fp == NULL)
		return -1;

	int i;
	for (i = 0; i < max_num_keys; i++) {
		if (fscanf(fp, "%20c\n", (char *)&key_array[i]) != 1)
			break;
	}

	fclose(fp);
	return i;
}

void chord_update_range(Server *srv, chordID *l, chordID *r)
{
	printf("update_range(");
	print_chordID(l);
	printf(" - ");
	print_chordID(r);
	printf(")\n");

	srv->pred_bound = *l;
	srv->node.id = *r;
}

/* get_range: returns the range (l,r] that this node is responsible for */
void chord_get_range(Server *srv, chordID *l, chordID *r)
{
	*l = srv->pred_bound;
	*r = srv->node.id;
}

int chord_is_local(Server *srv, chordID *x)
{
	return equals(x, &srv->node.id) || is_between(x, &srv->pred_bound,
												  &srv->node.id);
}

void chord_set_packet_handler(Server *srv, int event,
							  chord_packet_handler handler)
{
	srv->packet_handlers[event & 0x0F] = handler;
}

void chord_set_packet_handler_ctx(Server *srv, void *ctx)
{
	srv->packet_handler_ctx = ctx;
}
