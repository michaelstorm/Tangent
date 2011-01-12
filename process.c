#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "chord.h"

int process_data(Server *srv, uchar type, byte ttl, chordID *id, ushort len,
				 uchar *data)
{
	Node *np;
	Finger *pf, *sf;

	CHORD_DEBUG(5, print_process(srv, "process_data", id, -1, -1));

	if (--ttl == 0) {
		print_two_chordIDs("TTL expired: data packet ", id, " dropped at node ",
						   &srv->node.id, "\n");
		return CHORD_TTL_EXPIRED;
	}

	/* handle request locally? */
	if (chord_is_local(id)) {
		/* Upcall goes here... */
		chord_deliver(len, data);
		return 1;
	}

	if ((type == CHORD_ROUTE_LAST) && ((pf = pred_finger(srv)) != NULL)) {
		/* the previous hop N believes we are responsible for id,
		 * but we aren't. This means that our predecessor is
		 * a better successor for N. Just pass the packet to our
		 * predecessor. Note that ttl takes care of loops!
		 */
		send_data(srv, CHORD_ROUTE_LAST, ttl, &pf->node, id, len, data);
		return 1;
	}

	if ((sf = succ_finger(srv)) != NULL) {
		if (is_between(id, &srv->node.id, &sf->node.id)
			|| equals(id, &sf->node.id)) {
			/* according to our info the successor should be responsible
				 * for id; send the packet to the successor.
			 */
			send_data(srv, CHORD_ROUTE_LAST, ttl, &sf->node, id, len, data);
			return 1;
		}
	}
	/* send packet to the closest active predecessor (that we know about) */
	np = closest_preceding_node(srv, id, FALSE);
	send_data(srv, CHORD_ROUTE, ttl, np, id, len, data);
	return 1;
}

/**********************************************************************/

int process_fs(Server *srv, uchar *ticket, byte ttl, chordID *id, ulong addr,
			   ushort port)
{
	Node *succ, *np;

	CHORD_DEBUG(5, print_process(srv, "process_fs", id, addr, port));

	if (--ttl == 0) {
		print_two_chordIDs("TTL expired: fix_finger packet ", id,
						   " dropped at node ", &srv->node.id, "\n");
		return CHORD_TTL_EXPIRED;
	}

	if (srv->node.addr == addr && srv->node.port == port)
		return 1;

	if (succ_finger(srv) == NULL) {
		send_fs_repl(srv, ticket, addr, port, &srv->node.id, srv->node.addr,
					 srv->node.port);
		return 1;
	}
	succ = &(succ_finger(srv)->node);

	if (is_between(id, &srv->node.id, &succ->id) || equals(id, &succ->id))
		send_fs_repl(srv, ticket, addr, port, &succ->id, succ->addr,
					 succ->port);
	else {
		np = closest_preceding_node(srv, id, FALSE);
		send_fs_forward(srv, ticket, ttl, np->addr, np->port, id, addr, port);
	}
	return 1;
}

/**********************************************************************/

int process_fs_repl(Server *srv, uchar *ticket, chordID *id, ulong addr,
					ushort port)
{
	int fnew;

	if (!verify_ticket(&srv->ticket_key, ticket, "c", CHORD_FS))
		return CHORD_INVALID_TICKET;

	if (srv->node.addr == addr && srv->node.port == port)
		return 1;

	CHORD_DEBUG(5, print_process(srv, "process_fs_repl", id, -1, -1));

	insert_finger(srv, id, addr, port, &fnew);
	if (fnew == TRUE)
		send_ping(srv, addr, port, srv->node.addr, srv->node.port,
				  get_current_time());

	return 1;
}

/**********************************************************************/

int process_stab(Server *srv, chordID *id, ulong addr, ushort port)
{
	Finger *pred = pred_finger(srv);
	int		 fnew;

	CHORD_DEBUG(5, print_process(srv, "process_stab", id, addr, port));

	insert_finger(srv, id, addr, port, &fnew);

	// If we have a predecessor, tell the requesting node what it is.
	if (pred)
		send_stab_repl(srv, addr, port, &pred->node.id, pred->node.addr,
					   pred->node.port);
	return 1;
}

/**********************************************************************/

int process_stab_repl(Server *srv, chordID *id, ulong addr, ushort port)
{
	Finger *succ;
	int fnew;

	CHORD_DEBUG(5, print_process(srv, "process_stab_repl", id, -1, -1));

	// If we are our successor's predecessor, everything is fine, so do nothing.
	if ((srv->node.addr == addr) && (srv->node.port == port))
		return 1;

	// Otherwise, there is a better successor in between us and our current
	// successor. So we notify the in-between node that we should be its
	// predecessor.
	insert_finger(srv, id, addr, port, &fnew);
	succ = succ_finger(srv);
	send_notify(srv, succ->node.addr, succ->node.port, &srv->node.id,
				srv->node.addr, srv->node.port);
	if (fnew == TRUE)
		send_ping(srv, addr, port, srv->node.addr, srv->node.port,
				  get_current_time());
	return 1;
}

/**********************************************************************/

int process_notify(Server *srv, chordID *id, ulong addr, ushort port)
{
	int fnew;

	CHORD_DEBUG(5, print_process(srv, "process_notify", id, addr, port));

	// another node thinks that it should be our predecessor
	insert_finger(srv, id, addr, port, &fnew);
	if (fnew == TRUE)
		send_ping(srv, addr, port, srv->node.addr, srv->node.port,
				  get_current_time());
	return 1;
}

/**********************************************************************/

int process_ping(Server *srv, uchar *ticket, chordID *id, ulong addr,
				 ushort port, ulong time)
{
	int fnew;
	Finger *pred;

	CHORD_DEBUG(5, print_process(srv, "process_ping", id, addr, port));
	insert_finger(srv, id, addr, port, &fnew);
	pred = pred_finger(srv);
	if (fnew == TRUE
		&& ((pred == NULL)
			|| (pred && is_between(id, &pred->node.id,
								   &srv->node.id)))) {
		send_ping(srv, addr, port, srv->node.addr, srv->node.port,
				  get_current_time());
	}

	send_pong(srv, ticket, addr, port, time);

	return 1;
}

/**********************************************************************/

int process_pong(Server *srv, uchar *ticket, chordID *id, ulong addr,
				 ushort port, ulong time, host *from)
{
	Finger *f, *pred, *newpred;
	ulong	 new_rtt;
	int		 fnew;

	if (!verify_ticket(&srv->ticket_key, ticket, "clsl", CHORD_PING,
						  from->addr, from->port, time))
		return CHORD_INVALID_TICKET;

	f = insert_finger(srv, id, addr, port, &fnew);
	if (!f) {
		fprintf(stderr, "dropping pong\n");
		return 0;
	}

#ifdef HASH_PORT_WITH_ADDRESS
	if (!verify_address_id(&f->node.id, from->addr, from->port))
#else
	if (!verify_address_id(&f->node.id, from->addr))
#endif
		return CHORD_INVALID_ID;

	CHORD_DEBUG(5, print_process(srv, "process_pong", id, addr, port));
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
		chord_update_range(&newpred->node.id, &srv->node.id);

	return 1;
}

/**********************************************************************/

int process_fingers_get(Server *srv, uchar *ticket, ulong addr, ushort port,
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

	CHORD_DEBUG(5, print_process(srv, "process_traceroute", id, -1, -1));

	assert(ttl);
	ttl--;

	/* handle request locally? */
	if (chord_is_local(id) || (ttl == 0)) {
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
								 -1, -1));

	if (hops == 0)
		return CHORD_PROTOCOL_ERROR;
	hops--;
	send_traceroute_repl(srv, buf, ttl, hops, FALSE);
	return 1;
}
