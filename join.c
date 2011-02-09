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
		printf("\taddr=[%s]:%d\n", addr_str, port);

		/* resolve address */
		if (resolve_v6name(addr_str, &srv->well_known[srv->nknown].addr)) {
			weprintf("could not join well-known node [%s]:%d", addr_str, port);
			break;
		}

		srv->well_known[srv->nknown].port = (in_port_t)port;
		srv->nknown++;
	}

	if (srv->nknown == 0)
		printf("Didn't find any known hosts.");

	chord_update_range(&srv->node.id, &srv->node.id);
	set_stabilize_timer(srv);
	stabilize(srv);
}
