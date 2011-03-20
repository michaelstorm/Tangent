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

static void init_ticket_key(Server *srv);

Server *new_server(struct event_base *ev_base, int tunnel_sock)
{
	Server *srv = calloc(1, sizeof(Server));
	srv->to_fix_finger = NFINGERS-1;

	srv->tunnel_sock = tunnel_sock;
	//eventqueue_listen_socket(srv->tunnel_sock, srv, (socket_func)handle_packet);

	srv->ev_base = ev_base;

	init_ticket_key(srv);

	srv->stab_event = event_new(srv->ev_base, -1, EV_TIMEOUT|EV_PERSIST,
								stabilize, srv);

	srv->discover_addr_event = event_new(srv->ev_base, -1,
										 EV_TIMEOUT|EV_PERSIST, discover_addr,
										 srv);

	return srv;
}

void server_initialize_from_file(Server *srv, char *conf_file)
{
	char id[4*CHORD_ID_LEN];
	int ip_ver;

	FILE *fp = fopen(conf_file, "r");
	if (fp == NULL)
		eprintf("fopen(%s,\"r\") failed:", conf_file);
	if (fscanf(fp, "%d\n", &ip_ver) != 1)
		eprintf("Didn't find ip version in \"%s\"", conf_file);
	if (fscanf(fp, "%hd", (short *)&srv->node.port) != 1)
		eprintf("Didn't find port in \"%s\"", conf_file);
	if (fscanf(fp, " %s\n", id) != 1)
		eprintf("Didn't find id in \"%s\"", conf_file);
//	srv->node.id = atoid(id);

	srv->is_v6 = ip_ver == 6;

	char addr_str[INET6_ADDRSTRLEN+16];
	while (srv->nknown < MAX_WELLKNOWN && fscanf(fp, "%s\n", addr_str) == 1) {
		in6_addr addr;
		ushort port;

		if (srv->is_v6) {
			struct sockaddr_in6 sock_addr;
			int outlen = sizeof(sock_addr);
			if (evutil_parse_sockaddr_port(addr_str,
										   (struct sockaddr *)&sock_addr,
										   &outlen) != 0)
			{
				fprintf(stderr, "error parsing address and port \"%s\"\n",
						addr_str);
				exit(1);
			}

			v6_addr_copy(&addr, &sock_addr.sin6_addr);
			port = ntohs(sock_addr.sin6_port);
		}
		else {
			struct sockaddr_in sock_addr;
			int outlen = sizeof(sock_addr);
			if (evutil_parse_sockaddr_port(addr_str,
										   (struct sockaddr *)&sock_addr,
										   &outlen) != 0)
			{
				fprintf(stderr, "error parsing address and port \"%s\"\n",
						addr_str);
				exit(1);
			}

			to_v6addr(sock_addr.sin_addr.s_addr, &addr);
			port = ntohs(sock_addr.sin_port);
		}

		/* resolve address */
		if (resolve_v6name(v6addr_to_str(&addr),
						   &srv->well_known[srv->nknown].node.addr)) {
			weprintf("could not join well-known node [%s]:%d", addr_str, port);
			break;
		}

		srv->well_known[srv->nknown].node.port = (in_port_t)port;
		srv->nknown++;
	}

	if (srv->nknown == 0)
		fprintf(stderr, "Didn't find any known hosts.");
}

void server_start(Server *srv)
{
	struct timeval timeout;
	timeout.tv_sec = ADDR_DISCOVER_INTERVAL / 1000000UL;
	timeout.tv_usec = ADDR_DISCOVER_INTERVAL % 1000000UL;

	event_add(srv->discover_addr_event, &timeout);
	event_active(srv->discover_addr_event, EV_TIMEOUT, 1);
}

void server_initialize_socket(Server *srv)
{
	srv->sock = socket(srv->is_v6 ? AF_INET6 : AF_INET, SOCK_DGRAM, 0);
	if (srv->is_v6)
		chord_bind_v6socket(srv->sock, &in6addr_any, srv->node.port);
	else
		chord_bind_v4socket(srv->sock, INADDR_ANY, srv->node.port);
	set_socket_nonblocking(srv->sock);

	srv->sock_event = event_new(srv->ev_base, srv->sock, EV_READ|EV_PERSIST,
								handle_packet, srv);
	event_add(srv->sock_event, NULL);
}

void chord_main(char **conf_files, int nservers, int tunnel_sock)
{
	/*setprogname("chord");
	srandom(getpid() ^ time(0));

	init_global_eventqueue();

	Server *servers[nservers];
	int i;
	for (i = 0; i < nservers; i++) {
		servers[i] = new_server(tunnel_sock);
		server_initialize_from_file(servers[i], conf_files[i]);
		server_initialize_socket(servers[i]);
		server_start(servers[i]);
	}

	eventqueue_loop();*/
}

/**********************************************************************/

static void init_ticket_key(Server *srv)
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

/**********************************************************************/

/* handle_packet: snarf packet from network and dispatch */
void handle_packet(evutil_socket_t sock, short what, void *arg)
{
	Server *srv = arg;
	ssize_t packet_len;
	socklen_t from_len;
	Node from;
	byte buf[BUFSIZE];

	/* if this is a chord packet, the first 4 bytes should be 1111; otherwise
	   it's a UDT packet and we hand it back to be processed in this rather
	   inelegant fashion */
	if (recv(sock, buf, 2, MSG_PEEK) == 2 && *(unsigned short *)buf != 0xFFFF)
		return;

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
			return;
		}

		return;
	}

	get_address_id(&from.id, &from.addr, from.port);
	dispatch(srv, packet_len-2, buf+2, &from);
	return;
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
	fprintf(stderr, "update_range(");
	print_chordID(l);
	fprintf(stderr, " - ");
	print_chordID(r);
	fprintf(stderr, ")\n");

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
	srv->packet_handlers[event] = handler;
}

void chord_set_packet_handler_ctx(Server *srv, void *ctx)
{
	srv->packet_handler_ctx = ctx;
}
