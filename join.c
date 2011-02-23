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

	eventqueue_push(ADDR_DISCOVER_INTERVAL, srv, (event_func)discover_addr);
	return 0;
}

/* join: Send join messages to hosts in file */
void join(Server *srv, FILE *fp)
{
	char addr_str[INET6_ADDRSTRLEN+16], *p;
	ushort port;

	printf("joining well-known nodes:\n");
	while (srv->nknown < MAX_WELLKNOWN && fscanf(fp, "[%s\n", addr_str) == 1) {
		p = strstr(addr_str, "]:");
		assert(p != NULL);
		*p = '\0';
		p += 2;
		port = atoi(p);

		/* resolve address */
		if (resolve_v6name(addr_str, &srv->well_known[srv->nknown].node.addr)) {
			weprintf("could not join well-known node [%s]:%d", addr_str, port);
			break;
		}

		printf("\taddr=[%s]:%d\n",
			   v6addr_to_str(&srv->well_known[srv->nknown].node.addr), port);

		srv->well_known[srv->nknown].node.port = (in_port_t)port;
		srv->nknown++;
	}

	if (srv->nknown == 0)
		printf("Didn't find any known hosts.");
}
