/* Chord server loop */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <openssl/rand.h>
#include "chord.h"
#include "dispatcher.h"
#include "grid.h"
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

	srv->dispatcher = new_dispatcher(CHORD_PONG+1);

#ifdef CHORD_MESSAGE_DEBUG
	dispatcher_set_debug(srv->dispatcher, 1);
#endif

	dispatcher_set_error_handlers(srv->dispatcher, NULL,
								  (process_error_fn)process_error);

	dispatcher_set_packet(srv->dispatcher, CHORD_ADDR_DISCOVER, srv,
						  addr_discover__unpack, process_addr_discover);
	dispatcher_set_packet(srv->dispatcher, CHORD_ADDR_DISCOVER_REPLY, srv,
						  addr_discover_reply__unpack,
						  process_addr_discover_reply);
	dispatcher_set_packet(srv->dispatcher, CHORD_DATA, srv, data__unpack,
						  process_data);
	dispatcher_set_packet(srv->dispatcher, CHORD_FS, srv,
						  find_successor__unpack, process_fs);
	dispatcher_set_packet(srv->dispatcher, CHORD_FS_REPLY, srv,
						  find_successor_reply__unpack, process_fs_reply);
	dispatcher_set_packet(srv->dispatcher, CHORD_STAB, srv, stabilize__unpack,
						  process_stab);
	dispatcher_set_packet(srv->dispatcher, CHORD_STAB_REPLY, srv,
						  stabilize_reply__unpack, process_stab_reply);
	dispatcher_set_packet(srv->dispatcher, CHORD_NOTIFY, srv, notify__unpack,
						  process_notify);
	dispatcher_set_packet(srv->dispatcher, CHORD_PING, srv, ping__unpack,
						  process_ping);
	dispatcher_set_packet(srv->dispatcher, CHORD_PONG, srv, pong__unpack,
						  process_pong);

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

	srv->ticket_salt = malloc(TICKET_SALT_LEN);
	srv->ticket_salt_len = TICKET_SALT_LEN;
	srv->ticket_hash_len = TICKET_HASH_LEN;
	if (!RAND_bytes(srv->ticket_salt, TICKET_SALT_LEN)) {
		fprintf(stderr, "Could not generate ticket key.\n");
		exit(2);
	}
}

/**********************************************************************/

/* handle_packet: snarf packet from network and dispatch */
void handle_packet(evutil_socket_t sock, short what, void *arg)
{
	Server *srv = arg;
	ssize_t packet_len;
	socklen_t from_len;
	Node from;
	uchar buf[BUFSIZE];

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
		switch (errno) {
		default:
			weprintf("recvfrom failed:");
			// fall through
		case EAGAIN:
		case EINTR:
			return;
		}
	}

	get_address_id(&from.id, &from.addr, from.port);

	if (!dispatch_packet(srv->dispatcher, buf, packet_len, &from, NULL))
		weprintf("dropped unknown packet type 0x%02x", buf[0]);
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

long double id_to_radians(const chordID *id)
{
	int i;
	long double rad = 0.0;

	for (i = 0; i < CHORD_ID_LEN; i++) {
		long double numerator = id->x[i];
		long double denominator = powl(256, i+1);
		rad += numerator/denominator;
	}
	return rad*2*PI;
}

void chord_print_circle(Server *srv)
{
	return;
#define ROW_RATIO ((long double)0.5)
#define DIAM ((long double)50)
	struct grid *g = new_grid(DIAM+1, DIAM*ROW_RATIO+1);

	struct circle *c = new_circle(DIAM/2, DIAM/2, DIAM, PI, ROW_RATIO, 4);
	draw_circle(g, c, '.');

	long double from = id_to_radians(&srv->pred_bound);
	long double to = id_to_radians(&srv->node.id);

	draw_arc(g, c, '*', from, to);
	draw_radius(g, c, '^', from);
	draw_radius(g, c, '^', to);

	Finger *f;
	for (f = srv->head_flist; f != NULL; f = f->next) {
		long double pos = id_to_radians(&f->node.id);
		if (f->status == F_PASSIVE)
			draw_circle_point(g, c, 'P', pos);
		else
			draw_circle_point(g, c, 'A', pos);
	}

	char addr_str[INET6_ADDRSTRLEN+16];
	if (!V4_MAPPED(&srv->node.addr)) {
		strcpy(addr_str, "[");
		strncat(addr_str, v6addr_to_str(&srv->node.addr), sizeof(addr_str)-1);

		char port_str[16];
		sprintf(port_str, "]:%d", srv->node.port);
		strncat(addr_str, port_str, sizeof(port_str)-1);
	}
	else {
		addr_str[0] = '\0';
		strncat(addr_str, v6addr_to_str(&srv->node.addr), sizeof(addr_str));

		char port_str[16];
		sprintf(port_str, ":%d", srv->node.port);
		strncat(addr_str, port_str, sizeof(port_str)-1);
	}

	char center_text[128];
	strcpy(center_text, addr_str);
	strcat(center_text, "\n");
	int id_start = strlen(center_text);

	int i;
	for (i = 0; i < 4; i++)
		sprintf(center_text+id_start+(i*2), "%02x", srv->pred_bound.x[i]);
	center_text[id_start+(i*2)] = '\0';

	strcat(center_text, " - ");
	id_start = strlen(center_text);

	for (i = 0; i < 4; i++)
		sprintf(center_text+id_start+(i*2), "%02x", srv->node.id.x[i]);
	center_text[id_start+(i*2)] = '\0';

	int text_width, text_height;
	measure_text(center_text, &text_width, &text_height);

	long double x = c->center_x-(((long double)(text_width+2))/2);
	long double y = (c->center_y*ROW_RATIO)+(((long double)(text_height+2))/2);
	draw_centered_text(g, center_text, lrintl(x), lrintl(y), 1);

	print_grid(stderr, g);
	free_circle(c);
	free_grid(g);
}

void chord_update_range(Server *srv, chordID *l, chordID *r)
{
	fprintf(stderr, "range: ");
	print_chordID(l);
	fprintf(stderr, " - ");
	print_chordID(r);
	fprintf(stderr, "\n");

	srv->pred_bound = *l;
	srv->node.id = *r;

	chord_print_circle(srv);
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
