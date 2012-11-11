/* Common API functions */

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "chord.h"

/**********************************************************************/

/* init: initialize chord server, return socket descriptor */
int chord_init(char **conf_files, int nservers)
{
	int tunnel[2];
	if (socketpair(AF_UNIX, SOCK_DGRAM, 0, tunnel) < 0)
		eprintf("socket_pair failed:");

	/* Catch all crashes/kills and cleanup */
	signal(SIGHUP, chord_cleanup);
	signal(SIGINT, chord_cleanup);
	signal(SIGILL, chord_cleanup);
	signal(SIGABRT, chord_cleanup);
	signal(SIGALRM, chord_cleanup);
	signal(SIGFPE, chord_cleanup);
	signal(SIGSEGV, chord_cleanup);
	signal(SIGPIPE, chord_cleanup);
	signal(SIGTERM, chord_cleanup);
	signal(SIGCHLD, chord_cleanup); /* If Chord process dies, exit */
	signal(SIGBUS, chord_cleanup);

	//if (!fork())		/* child */
		chord_main(conf_files, nservers, tunnel[1]);

	return tunnel[0];
}

/**********************************************************************/

void chord_cleanup(int signum)
{
	signal(SIGABRT, SIG_DFL);
	abort();
}
