#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <udt>
#include <unistd.h>
#include <pthread.h>
#include <event2/event.h>
#include <event2/thread.h>
#include "chord.h"
#include "dhash.h"
#include "pack.h"
#include "send.h"
#include "transfer.h"

DHash *new_dhash(const char *files_path)
{
	DHash *dhash = (DHash *)malloc(sizeof(DHash));
	dhash->servers = NULL;
	dhash->nservers = 0;
	dhash->trans_head = NULL;

	dhash->files_path = (char *)malloc(strlen(files_path)+1);
	strcpy(dhash->files_path, files_path);
	return dhash;
}

int dhash_stat_local_file(DHash *dhash, const char *file, struct stat *stat_buf)
{
	char abs_file_path[strlen(dhash->files_path) + strlen(file) + 1];

	strcpy(abs_file_path, dhash->files_path);
	strcpy(abs_file_path + strlen(dhash->files_path), "/");
	strcpy(abs_file_path + strlen(dhash->files_path)+1, file);

	return stat(abs_file_path, stat_buf);
}

int dhash_local_file_exists(DHash *dhash, const char *file)
{
	struct stat stat_buf;
	return dhash_stat_local_file(dhash, file, &stat_buf) == 0;
}

int dhash_local_file_size(DHash *dhash, const char *file)
{
	struct stat stat_buf;
	assert(dhash_stat_local_file(dhash, file, &stat_buf) == 0);
	return stat_buf.st_size;
}

int dhash_start(DHash *dhash, char **conf_files, int nservers)
{
	int dhash_tunnel[2];
	if (socketpair(AF_UNIX, SOCK_DGRAM, 0, dhash_tunnel) < 0)
		eprintf("socket_pair failed:");

	dhash->control_sock = dhash_tunnel[1];

	int f = fork();
	if (f) {
		fprintf(stderr, "child PID: %d\nparent PID: %d\n", f, getpid());
		return dhash_tunnel[0];
	}

	setprogname("dhash");
	srandom(getpid() ^ time(0));

	UDT::startup();

	evthread_use_pthreads();
	dhash->ev_base = event_base_new();

	dhash->servers = (Server **)malloc(sizeof(Server *)*nservers);
	dhash->chord_tunnel_socks = (int *)malloc(sizeof(int)*nservers);
	dhash->nservers = nservers;

	int i;
	for (i = 0; i < nservers; i++) {
		/*int chord_tunnel[2];
		if (socketpair(AF_UNIX, SOCK_DGRAM, 0, chord_tunnel) < 0)
			eprintf("socket_pair failed:");*/

		dhash->servers[i] = new_server(dhash->ev_base, 0 /*chord_tunnel[1]*/);
		dhash->chord_tunnel_socks[i] = 0 /*chord_tunnel[0]*/;

		Server *srv = dhash->servers[i];
		server_initialize_from_file(srv, conf_files[i]);
		server_initialize_socket(srv);

		chord_set_packet_handler(srv, CHORD_ROUTE,
								 (chord_packet_handler)dhash_unpack_chord_packet);
		chord_set_packet_handler(srv, CHORD_ROUTE_LAST,
								 (chord_packet_handler)dhash_unpack_chord_packet);
		chord_set_packet_handler_ctx(srv, dhash);

		server_start(srv);
	}

	dhash->control_sock_event = event_new(dhash->ev_base, dhash->control_sock,
										  EV_READ|EV_PERSIST,
										  dhash_unpack_control_packet, dhash);
	event_add(dhash->control_sock_event, NULL);

	event_base_dispatch(dhash->ev_base);
}
