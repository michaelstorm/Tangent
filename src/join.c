#include "chord/chord.h"
#include "chord/sendpkt.h"
#include "chord/util.h"
#include "sglib.h"

void discover_addr(evutil_socket_t sock, short what, void *arg)
{
	clog_set_event_context("discover_addr");

	Info("Discovering address");
	
	ChordServer *srv = arg;
	if (!IN6_IS_ADDR_UNSPECIFIED(&srv->node.addr)) {
		Error("called discover_addr when address is already known");
		return;
	}

	SGLIB_LIST_MAP_ON_ELEMENTS(struct Node, srv->well_known, node, next, {
		StartLog(DEBUG);
		PartialLog("Querying ");
		print_node(clog_file_logger()->fp, node);
		EndLog();
		send_addr_discover(srv, &node->addr, node->port);
	});

	clog_clear_event_context();
}
