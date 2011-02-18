/* Common API functions */

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "chord.h"

static int tunnel[2]; /* Socket pair for communication between the two layers */

/* route: forward message M towards the root of key K. */
void chord_route(chordID *k, char *data, int len)
{
	byte buf[BUFSIZE];

	if (send(tunnel[0], buf, pack_data(buf, CHORD_ROUTE, DEF_TTL, k, len, data),
			 0) < 0)
		weprintf("send failed:");		/* ignore errors */
}

/**********************************************************************/

/* init: initialize chord server, return socket descriptor */
int chord_init(char **conf_files, int nservers)
{
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

/**********************************************************************/

/* deliver: upcall */
void chord_deliver(int n, uchar *data, Node *from)
{
	/* Convert to I3 format... by stripping off the Chord header */
	send(tunnel[1], data, n, 0);
}
