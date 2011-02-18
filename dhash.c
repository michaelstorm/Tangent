#include <dirent.h>
#include <stdio.h>
#include "dhash.h"

#if 0
DHash *new_dhash(const char *files_path)
{
	DHash *dhash = malloc(sizeof(DHash));
	dhash->files_path = files_path;
	return dhash; // FIX
}

void dhash_read_dir()
{
	DIR *dir;
	struct dirent *ent;
	dir = opendir(files_path);
	if (dir != NULL) {
		/* print all the files and directories within directory */
		while ((ent = readdir(dir)) != NULL)
			printf("%s\n", ent->d_name);
		closedir(dir);
	}
	else {
		/* could not open directory */
		perror("");
		return NULL;
	}
}

int dhash_start(DHash *dhash, char **conf_files, int nservers)
{
	int tunnel[2];
	if (socketpair(AF_UNIX, SOCK_DGRAM, 0, tunnel) < 0)
		eprintf("socket_pair failed:");

	int i;
	int nfds = 0;
	fd_set interesting;
	setprogname("chord");
	srandom(getpid() ^ time(0));
	FD_ZERO(&interesting);

	Server *servers[nservers];
	for (i = 0; i < nservers; i++) {
		Server *srv = new_server(conf_files[i], tunnel_sock);
		servers[i] = srv;

		FD_SET(srv->sock, &interesting);
		FD_SET(srv->tunnel_sock, &interesting);
		nfds = MAX(nfds-1, MAX(srv->sock, srv->tunnel_sock)) + 1;
	}

	for (;;) {
		/* find the soonest-scheduled event among the servers */
		struct timeval timeout;
		int64_t stabilize_wait = INT64_MAX;
		uint64_t wtime = wall_time();
		fprintf(stderr, "wall_time: %llu\n", wtime);
		for (i = 0; i < nservers; i++) {
			Server *srv = servers[i];
			stabilize_wait = MIN(stabilize_wait,
								 (int64_t)(srv->next_event_us - wtime));
			if (stabilize_wait < 0) {
				stabilize_wait = 0;
				break;
			}
		}
		timeout.tv_sec = stabilize_wait / 1000000UL;
		timeout.tv_usec = stabilize_wait % 1000000UL;

		fd_set readable = interesting;
		int nfound = select(nfds, &readable, NULL, NULL, &timeout);
		/* error */
		if (nfound < 0 && errno == EINTR)
			continue;
		/* no readable sockets, so one of the timers fired */
		else if (nfound == 0) {
			/* poll all servers; protects against skipping servers due to clock
			 * jitter if some have timers firing close together */
			wtime = wall_time();
			for (i = 0; i < nservers; i++) {
				Server *srv = servers[i];
				int64_t stabilize_wait = (int64_t)(srv->next_event_us
												   - wtime);
				if (stabilize_wait <= 0)
					chord_process_events(srv);
			}
		}
		/* one or more packets arrived */
		else {
			for (i = 0; i < nservers; i++) {
				Server *srv = servers[i];
				if (FD_ISSET(srv->sock, &readable))
					handle_packet(srv, srv->sock);
				if (FD_ISSET(srv->tunnel_sock, &readable))
					handle_packet(srv, srv->tunnel_sock);
			}
		}
	}
}

void dhash_request_file(DHash *dhash, const char *file)
{

}

#endif
