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

int discover_addr(Server *srv)
{
	if (!IN6_IS_ADDR_UNSPECIFIED(&srv->node.addr))
		return 0;

	int i;
	for (i = 0; i < srv->nknown; i++) {
		send_addr_discover(srv, &srv->well_known[i].node.addr,
						   srv->well_known[i].node.port);
	}

	eventqueue_push_timer(ADDR_DISCOVER_INTERVAL, srv,
						  (timer_func)discover_addr);
	return 0;
}
