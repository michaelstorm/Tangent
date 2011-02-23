#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include "chord.h"

int process_addr_discover(Server *srv, uchar *ticket, Node *from)
{
	CHORD_DEBUG(5, print_process(srv, "process_addr_discover", &from->id,
								 &from->addr, from->port));

	send_addr_discover_repl(srv, ticket, &from->addr, from->port);
	return 1;
}

int process_addr_discover_repl(Server *srv, uchar *ticket, in6_addr *addr,
							   Node *from)
{
	CHORD_DEBUG(5, print_process(srv, "process_addr_discover_repl", &from->id,
								 &from->addr, from->port));

	verify_ticket(&srv->ticket_key, ticket, "c6s", CHORD_ADDR_DISCOVER,
				  &from->addr, from->port);

	if (IN6_IS_ADDR_UNSPECIFIED(&srv->node.addr)) {
		v6_addr_copy(&srv->node.addr, addr);
		get_address_id(&srv->node.id, &srv->node.addr, srv->node.port);
		chord_update_range(srv, &srv->node.id, &srv->node.id);

		stabilize(srv);
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

int process_data(Server *srv, uchar type, byte ttl, chordID *id, ushort len,
				 uchar *data, Node *from)
{
	CHORD_DEBUG(3, print_process(srv, "process_data", id, NULL, -1));

	if (--ttl == 0) {
		print_two_chordIDs("TTL expired: data packet ", id, " dropped at node ",
						   &srv->node.id, "\n");
		return CHORD_TTL_EXPIRED;
	}

	/* handle request locally? */
	if (chord_is_local(srv, id)) {
		/* Upcall goes here... */
		printf("id is local\n");
		//chord_deliver(len, data, from);
	}
	else {
		uchar route_type;
		Node *np = next_route_node(srv, id, type, &route_type);
		send_data(srv, route_type, ttl, np, id, len, data);
	}
	return 1;
}

/**********************************************************************/

int process_fs(Server *srv, uchar *ticket, byte ttl, in6_addr *reply_addr,
			   ushort reply_port)
{
	Node *succ, *np;
	chordID reply_id;

	get_address_id(&reply_id, reply_addr, reply_port);

	CHORD_DEBUG(5, print_process(srv, "process_fs", &reply_id, reply_addr,
								 reply_port));

	if (--ttl == 0) {
		print_two_chordIDs("TTL expired: fix_finger packet ", &reply_id,
						   " dropped at node ", &srv->node.id, "\n");
		return CHORD_TTL_EXPIRED;
	}

	if (v6_addr_equals(&srv->node.addr, reply_addr)
		&& srv->node.port == reply_port)
		return 1;

	if (succ_finger(srv) == NULL) {
		send_fs_repl(srv, ticket, reply_addr, reply_port, &srv->node.addr,
					 srv->node.port);
		return 1;
	}
	succ = &(succ_finger(srv)->node);

	if (is_between(&reply_id, &srv->node.id, &succ->id) || equals(&reply_id,
																  &succ->id))
		send_fs_repl(srv, ticket, reply_addr, reply_port, &succ->addr,
					 succ->port);
	else {
		np = closest_preceding_node(srv, &reply_id, FALSE);
		send_fs_forward(srv, ticket, ttl, &np->addr, np->port, reply_addr,
						reply_port);
	}
	return 1;
}

/**********************************************************************/

int process_fs_repl(Server *srv, uchar *ticket, in6_addr *addr, ushort port)
{
	int fnew;
	chordID id;

	get_address_id(&id, addr, port);

	if (!verify_ticket(&srv->ticket_key, ticket, "c", CHORD_FS))
		return CHORD_INVALID_TICKET;

	if (v6_addr_equals(&srv->node.addr, addr) && srv->node.port == port)
		return 1;

	CHORD_DEBUG(5, print_process(srv, "process_fs_repl", &id, NULL, -1));

	insert_finger(srv, &id, addr, port, &fnew);
	if (fnew == TRUE)
		send_ping(srv, addr, port, get_current_time());

	return 1;
}

/**********************************************************************/

int process_stab(Server *srv, in6_addr *addr, ushort port)
{
	Finger *pred = pred_finger(srv);
	int		 fnew;
	chordID id;

	get_address_id(&id, addr, port);

	CHORD_DEBUG(5, print_process(srv, "process_stab", &id, addr, port));

	insert_finger(srv, &id, addr, port, &fnew);

	// If we have a predecessor, tell the requesting node what it is.
	if (pred)
		send_stab_repl(srv, addr, port, &pred->node.addr, pred->node.port);
	return 1;
}

/**********************************************************************/

int process_stab_repl(Server *srv, in6_addr *addr, ushort port)
{
	Finger *succ;
	int fnew;
	chordID id;

	get_address_id(&id, addr, port);

	CHORD_DEBUG(5, print_process(srv, "process_stab_repl", &id, NULL, -1));

	// If we are our successor's predecessor, everything is fine, so do nothing.
	if (v6_addr_equals(&srv->node.addr, addr) && srv->node.port == port)
		return 1;

	// Otherwise, there is a better successor in between us and our current
	// successor. So we notify the in-between node that we should be its
	// predecessor.
	insert_finger(srv, &id, addr, port, &fnew);
	succ = succ_finger(srv);
	send_notify(srv, &succ->node.addr, succ->node.port);
	if (fnew == TRUE)
		send_ping(srv, addr, port, get_current_time());
	return 1;
}

/**********************************************************************/

int process_notify(Server *srv, Node *from)
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

int process_ping(Server *srv, uchar *ticket, ulong time, Node *from)
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

	send_pong(srv, ticket, &from->addr, from->port, time);

	return 1;
}

/**********************************************************************/

int process_pong(Server *srv, uchar *ticket, ulong time, Node *from)
{
	Finger *f, *pred, *newpred;
	ulong	 new_rtt;
	int		 fnew;

	if (!verify_ticket(&srv->ticket_key, ticket, "c6sl", CHORD_PING,
					   &from->addr, from->port, time))
		return CHORD_INVALID_TICKET;

	f = insert_finger(srv, &from->id, &from->addr, from->port, &fnew);
	if (!f) {
		fprintf(stderr, "dropping pong\n");
		return 0;
	}

	CHORD_DEBUG(5, print_process(srv, "process_pong", &from->id, &from->addr,
								 from->port));
	f->npings = 0;
	new_rtt = get_current_time() - time; /* takes care of overlow */
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

/**********************************************************************/

int process_fingers_get(Server *srv, uchar *ticket, in6_addr *addr, ushort port,
						chordID *key)
{
	CHORD_DEBUG(5, print_process(srv, "process_fingers_get", NULL, addr, port));
	send_fingers_repl(srv, ticket, addr, port);

	return 1;
}

/**********************************************************************/

int process_fingers_repl(Server *srv, uchar ret_code)
{
	/* this process should be never invoked by the i3 server,
	 * as CHORD_FINGERS_REPL is always sent to the client
	 */
	CHORD_DEBUG(5, print_process(srv, "process_fingers_repl", NULL, 0, 0));
	return 1;
}

/**********************************************************************/

int process_traceroute(Server *srv, chordID *id, char *buf, uchar type,
					   byte ttl, byte hops)
{
	Finger *f;

	CHORD_DEBUG(5, print_process(srv, "process_traceroute", id, NULL, -1));

	assert(ttl);
	ttl--;

	/* handle request locally? */
	if (chord_is_local(srv, id) || (ttl == 0)) {
		send_traceroute_repl(srv, buf, ttl, hops, (hops ? FALSE : TRUE));
		return 1;
	}
	hops++;
	if ((type == CHORD_TRACEROUTE_LAST) && (f = pred_finger(srv))) {
		/* the previous hop N believes we are responsible for id,
		 * but we aren't. This means that our predecessor is
		 * a better successor for N. Just pass the packet to our
		 * predecessor. Note that ttl takes care of loops.
		 */
		send_traceroute(srv, f, buf, CHORD_TRACEROUTE_LAST, ttl, hops);
		return 1;
	}
	if ((f = succ_finger(srv)) != NULL) {
		if (is_between(id, &srv->node.id, &f->node.id)
			|| equals(id, &f->node.id)) {
			send_traceroute(srv, f, buf, CHORD_TRACEROUTE_LAST, ttl, hops);
			return 1;
		}
	}

	/* send to the closest predecessor (that we know about) */
	f = closest_preceding_finger(srv, id, FALSE);
	send_traceroute(srv, f, buf, CHORD_TRACEROUTE, ttl, hops);
	return 1;
}

/**********************************************************************/

int process_traceroute_repl(Server *srv, char *buf, byte ttl, byte hops)
{
	CHORD_DEBUG(5, print_process(srv, "process_traceroute_repl", &srv->node.id,
								 NULL, -1));

	if (hops == 0)
		return CHORD_PROTOCOL_ERROR;
	hops--;
	send_traceroute_repl(srv, buf, ttl, hops, FALSE);
	return 1;
}
