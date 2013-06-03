#include "chord/chord.h"
#include "chord/sendpkt.h"

void discover_addr(evutil_socket_t sock, short what, void *arg)
{
	Log(INFO, "Discovering address");
	
	ChordServer *srv = arg;
	if (!IN6_IS_ADDR_UNSPECIFIED(&srv->node.addr)) {
		Error("called discover_addr when address is already known");
		return;
	}

	int i;
	for (i = 0; i < srv->nknown; i++) {
		send_addr_discover(srv, &srv->well_known[i].node.addr,
						   srv->well_known[i].node.port);
	}
}
