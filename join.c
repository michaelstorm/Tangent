#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/utsname.h>
#include "chord.h"
#include "eventloop.h"

void discover_addr(evutil_socket_t sock, short what, void *arg)
{
	Server *srv = arg;
	if (!IN6_IS_ADDR_UNSPECIFIED(&srv->node.addr)) {
		fprintf(stderr, "called discover_addr when address is already known\n");
		return;
	}

	int i;
	for (i = 0; i < srv->nknown; i++) {
		send_addr_discover(srv, &srv->well_known[i].node.addr,
						   srv->well_known[i].node.port);
	}
}
