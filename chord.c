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
	FILE *fp;

	Server *srv = malloc(sizeof(Server));
	srv->tunnel_sock = tunnel_sock;

	srv->nknown = 0;
	srv->num_passive_fingers = 0;

	memset(srv, 0, sizeof(Server));
	srv->to_fix_finger = NFINGERS-1;

	fp = fopen(conf_file, "r");
	if (fp == NULL)
		eprintf("fopen(%s,\"r\") failed:", conf_file);
	if (fscanf(fp, "%hd", (short *)&srv->node.port) != 1)
		eprintf("Didn't find port in \"%s\"", conf_file);
	if (fscanf(fp, " %s\n", id) != 1)
		eprintf("Didn't find id in \"%s\"", conf_file);
	srv->node.id = atoid(id);

	/* Figure out one's own address somehow */
	to_v6addr(get_addr(), &srv->node.addr);

	fprintf(stderr, "Chord started.\n");
	fprintf(stderr, "id="); print_id(stderr, &srv->node.id);
	fprintf(stderr, "\n");

	fprintf(stderr, "ip=%s\n", v6addr_to_str(&srv->node.addr));
	fprintf(stderr, "port=%d\n", srv->node.port);

	initialize(srv);
	join(srv, fp);
	fclose(fp);

	FD_ZERO(&srv->interesting);
	FD_SET(srv->sock, &srv->interesting);
	FD_SET(srv->tunnel_sock, &srv->interesting);
	srv->nfds = MAX(srv->sock, tunnel_sock) + 1;
}

int chord_process_events(Server *srv)
{
	struct timeval timeout;
	fd_set readable = srv->interesting;
	int64_t stabilize_wait = (int64_t)(srv->next_stabilize_us - wall_time());
	stabilize_wait = MAX(stabilize_wait, 0);
	timeout.tv_sec = stabilize_wait / 1000000UL;
	timeout.tv_usec = stabilize_wait % 1000000UL;

	int nfound = select(srv->nfds, &readable, NULL, NULL, &timeout);
	if (nfound < 0 && errno == EINTR)
		return 0;
	else if (nfound == 0) {
		stabilize_wait = (int64_t)(srv->next_stabilize_us - wall_time());
		if (stabilize_wait <= 0)
			stabilize(srv);
		return 0;
	}

	if (FD_ISSET(srv->sock, &readable))
		handle_packet(srv, srv->sock);
	if (FD_ISSET(srv->tunnel_sock, &readable))
		handle_packet(srv, srv->tunnel_sock);

	return 1;
}

void chord_main(char *conf_file, int tunnel_sock)
{
	int nfound;
	int64_t stabilize_wait;
	struct timeval timeout;

	setprogname("chord");
	srandom(getpid() ^ time(0));

	Server *srv = new_server(conf_file, tunnel_sock);

	/* Loop on input */
	for (;;)
		chord_process_events(srv);
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
void initialize(Server *srv)
{
	setservent(1);

	srv->sock = init_socket4(INADDR_ANY, srv->node.port);
	set_socket_nonblocking(srv->sock);

	srv->is_v6 = 0;

	init_ticket_key(srv);
}

/**********************************************************************/

/* handle_packet: snarf packet from network and dispatch */
void handle_packet(Server *srv, int sock)
{
	ssize_t packet_len;
	socklen_t from_len;
	struct sockaddr_in from_sa;
	byte buf[BUFSIZE];

	from_len = sizeof(from_sa);
	packet_len = recvfrom(sock, buf, sizeof(buf), 0,
						  (struct sockaddr *)&from_sa, &from_len);
	if (packet_len < 0) {
		if (errno != EAGAIN) {
			weprintf("recvfrom failed:"); /* ignore errors for now */
			return;
		}

		weprintf("handle_packet: EAGAIN");
		return; /* pick up this packet later */
	}

	host from;
	to_v6addr(from_sa.sin_addr.s_addr, &from.addr);
	from.port = ntohs(from_sa.sin_port);
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
