#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include "chord.h"
#include "messages.pb-c.h"

int process_addr_discover(Server *srv, AddrDiscover *msg, Node *from)
{
	CHORD_DEBUG(5, print_process(srv, "process_addr_discover", &from->id,
								 &from->addr, from->port));

	send_addr_discover_reply(srv, msg->ticket.data, &from->addr, from->port);
	return 1;
}

int process_addr_discover_reply(Server *srv, AddrDiscoverReply *msg, Node *from)
{
	CHORD_DEBUG(5, print_process(srv, "process_addr_discover_repl", &from->id,
								 &from->addr, from->port));

	if (!verify_ticket(&srv->ticket_key, msg->ticket.data, "c6s",
					   CHORD_ADDR_DISCOVER, &from->addr, from->port))
		return CHORD_INVALID_TICKET;

	if (IN6_IS_ADDR_UNSPECIFIED(&srv->node.addr)) {
		v6_addr_set(&srv->node.addr, msg->addr.data);
		get_address_id(&srv->node.id, &srv->node.addr, srv->node.port);
		chord_update_range(srv, &srv->node.id, &srv->node.id);

		fprintf(stderr, "address: [%s]:%d\n", v6addr_to_str(&srv->node.addr),
			   srv->node.port);
		fprintf(stderr, "node id: ");
		print_chordID(&srv->node.id);
		fprintf(stderr, "\n");

		event_del(srv->discover_addr_event);

		struct timeval timeout;
		timeout.tv_sec = STABILIZE_PERIOD / 1000000UL;
		timeout.tv_usec = STABILIZE_PERIOD % 1000000UL;

		event_add(srv->stab_event, &timeout);
		event_active(srv->stab_event, EV_TIMEOUT, 1);
	}
	return 1;
}

Node *next_route_node(Server *srv, chordID *id, uchar pkt_type,
					  uchar *route_type)
{
	Finger *pf, *sf;

	if ((pkt_type == CHORD_ROUTE_LAST) && ((pf = pred_finger(srv)) != NULL)) {
		/* the previous hop N believes we are responsible for id,
		 * but we aren't. This means that our predecessor is
		 * a better successor for N. Just pass the packet to our
		 * predecessor. Note that ttl takes care of loops!
		 */
		*route_type = CHORD_ROUTE_LAST;
		return &pf->node;
	}

	if ((sf = succ_finger(srv)) != NULL) {
		if (is_between(id, &srv->node.id, &sf->node.id)
			|| equals(id, &sf->node.id)) {
			/* according to our info the successor should be responsible
				 * for id; send the packet to the successor.
			 */
			*route_type = CHORD_ROUTE_LAST;
			return &sf->node;
		}
	}

	/* send packet to the closest active predecessor (that we know about) */
	*route_type = CHORD_ROUTE;
	return closest_preceding_node(srv, id, FALSE);
}

static int process_data(Server *srv, int type, Data *msg, Node *from)
{
	chordID id;
	memcpy(id.x, msg->id.data, CHORD_ID_LEN);

	CHORD_DEBUG(3, print_process(srv, "process_data", &id, NULL, -1));

	if (--msg->ttl == 0) {
		print_two_chordIDs("TTL expired: data packet ", &id,
						   " dropped at node ", &srv->node.id, "\n");
		return CHORD_TTL_EXPIRED;
	}

	/* handle request locally? */
	if (chord_is_local(srv, &id)) {
		/* Upcall goes here... */
		fprintf(stderr, "id is local\n");
		//chord_deliver(len, data, from);
	}
	else {
		uchar route_type;
		Node *np = next_route_node(srv, &id, type, &route_type);
		send_data(srv, route_type, msg->ttl, np, &id, msg->data.len,
				  msg->data.data);
	}
	return 1;
}

int process_route(Server *srv, Data *msg, Node *from)
{
	return process_data(srv, CHORD_ROUTE, msg, from);
}

int process_route_last(Server *srv, Data *msg, Node *from)
{
	return process_data(srv, CHORD_ROUTE_LAST, msg, from);
}

/**********************************************************************/

int process_fs(Server *srv, FindSuccessor *msg, Node *from)
{
	Node *succ, *np;
	chordID reply_id;
	in6_addr reply_addr;
	ushort reply_port = msg->port;

	v6_addr_set(&reply_addr, msg->addr.data);
	get_address_id(&reply_id, &reply_addr, reply_port);

	CHORD_DEBUG(5, print_process(srv, "process_fs", &reply_id, &reply_addr,
								 reply_port));

	if (--msg->ttl == 0) {
		print_two_chordIDs("TTL expired: fix_finger packet ", &reply_id,
						   " dropped at node ", &srv->node.id, "\n");
		return CHORD_TTL_EXPIRED;
	}

	if (v6_addr_equals(&srv->node.addr, &reply_addr)
		&& srv->node.port == reply_port)
		return 1;

	if (succ_finger(srv) == NULL) {
		send_fs_reply(srv, msg->ticket.data, &reply_addr, reply_port,
					  &srv->node.addr, srv->node.port);
		return 1;
	}
	succ = &(succ_finger(srv)->node);

	if (is_between(&reply_id, &srv->node.id, &succ->id) || equals(&reply_id,
																  &succ->id)) {
		send_fs_reply(srv, msg->ticket.data, &reply_addr, reply_port,
					  &succ->addr, succ->port);
	}
	else {
		np = closest_preceding_node(srv, &reply_id, FALSE);
		send_fs_forward(srv, msg->ticket.data, msg->ttl, &np->addr, np->port,
						&reply_addr, reply_port);
	}
	return 1;
}

/**********************************************************************/

int process_fs_reply(Server *srv, FindSuccessorReply *msg, Node *from)
{
	int fnew;
	chordID id;

	in6_addr addr;
	memcpy(addr.s6_addr, msg->addr.data, 16);

	get_address_id(&id, &addr, msg->port);

	if (!verify_ticket(&srv->ticket_key, msg->ticket.data, "c", CHORD_FS))
		return CHORD_INVALID_TICKET;

	if (v6_addr_equals(&srv->node.addr, &addr) && srv->node.port == msg->port)
		return 1;

	CHORD_DEBUG(5, print_process(srv, "process_fs_repl", &id, &from->addr,
								 from->port));

	insert_finger(srv, &id, &addr, msg->port, &fnew);
	if (fnew == TRUE)
		send_ping(srv, &addr, msg->port, get_current_time());

	return 1;
}

/**********************************************************************/

int process_stab(Server *srv, Stabilize *msg, Node *from)
{
	Finger *pred = pred_finger(srv);
	int		 fnew;
	chordID id;

	in6_addr addr;
	memcpy(addr.s6_addr, msg->addr.data, 16);

	get_address_id(&id, &addr, msg->port);

	CHORD_DEBUG(5, print_process(srv, "process_stab", &id, &addr, msg->port));

	insert_finger(srv, &id, &addr, msg->port, &fnew);

	// If we have a predecessor, tell the requesting node what it is.
	if (pred)
		send_stab_reply(srv, &addr, msg->port, &pred->node.addr,
						pred->node.port);
	return 1;
}

/**********************************************************************/

int process_stab_reply(Server *srv, StabilizeReply *msg, Node *from)
{
	Finger *succ;
	int fnew;
	chordID id;

	in6_addr addr;
	memcpy(addr.s6_addr, msg->addr.data, 16);

	get_address_id(&id, &addr, msg->port);

	CHORD_DEBUG(5, print_process(srv, "process_stab_repl", &id, NULL, -1));

	// If we are our successor's predecessor, everything is fine, so do nothing.
	if (v6_addr_equals(&srv->node.addr, &addr) && srv->node.port == msg->port)
		return 1;

	// Otherwise, there is a better successor in between us and our current
	// successor. So we notify the in-between node that we should be its
	// predecessor.
	insert_finger(srv, &id, &addr, msg->port, &fnew);
	succ = succ_finger(srv);
	send_notify(srv, &succ->node.addr, succ->node.port);
	if (fnew == TRUE)
		send_ping(srv, &addr, msg->port, get_current_time());
	return 1;
}

/**********************************************************************/

int process_notify(Server *srv, Notify *msg, Node *from)
{
	int fnew;

	CHORD_DEBUG(5, print_process(srv, "process_notify", &from->id, &from->addr,
								 from->port));

	// another node thinks that it should be our predecessor
	insert_finger(srv, &from->id, &from->addr, from->port, &fnew);
	if (fnew == TRUE)
		send_ping(srv, &from->addr, from->port, get_current_time());
	return 1;
}

/**********************************************************************/

int process_ping(Server *srv, Ping *msg, Node *from)
{
	int fnew;
	Finger *pred;

	CHORD_DEBUG(5, print_process(srv, "process_ping", &from->id, &from->addr,
								 from->port));

	insert_finger(srv, &from->id, &from->addr, from->port, &fnew);
	pred = pred_finger(srv);
	if (fnew == TRUE
		&& ((pred == NULL)
			|| (pred && is_between(&from->id, &pred->node.id,
								   &srv->node.id)))) {
		send_ping(srv, &from->addr, from->port, get_current_time());
	}

	send_pong(srv, msg->ticket.data, &from->addr, from->port, msg->time);

	return 1;
}

/**********************************************************************/

int process_pong(Server *srv, Pong *msg, Node *from)
{
	Finger *f, *pred, *newpred;
	ulong	 new_rtt;
	int		 fnew;

	if (!verify_ticket(&srv->ticket_key, msg->ticket.data, "c6sl", CHORD_PING,
					   &from->addr, from->port, msg->time))
		return CHORD_INVALID_TICKET;

	f = insert_finger(srv, &from->id, &from->addr, from->port, &fnew);
	if (!f) {
		fprintf(stderr, "dropping pong\n");
		return 0;
	}

	CHORD_DEBUG(5, print_process(srv, "process_pong", &from->id, &from->addr,
								 from->port));
	f->npings = 0;
	new_rtt = get_current_time() - msg->time; /* takes care of overlow */
	update_rtt(&f->rtt_avg, &f->rtt_dev, (long)new_rtt);

	pred = pred_finger(srv);
	activate_finger(srv, f); /* there is a two-way connectivity to this node */
	newpred = pred_finger(srv); /* check whether pred has changed, i.e.,
								 * f has became the new pred
								 */
	assert(newpred || (pred == newpred));

	if (pred != newpred)
		chord_update_range(srv, &newpred->node.id, &srv->node.id);

	return 1;
}
