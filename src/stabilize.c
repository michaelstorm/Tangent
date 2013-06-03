#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "chord/chord.h"
#include "chord/finger.h"
#include "chord/sendpkt.h"
#include "chord/util.h"

/* local functions */
static void fix_fingers(ChordServer *srv);
static void fix_succs_preds(ChordServer *srv);
static void ping(ChordServer *srv);
static void clean_finger_list(ChordServer *srv);

/* stabilize: the following things are done periodically
 *	- stabilize successor by asking for its predecessor
 *	- fix one backup successor
 *	- fix one proper finger
 *	- ping one node in the finger list (the finger list includes
 *		the backup successors, the proper fingers and the predecessor)
 *	- ping any node in the finger list that has not replied to the
 *		previous ping
 */

#define CHORD_CLEAN_PERIOD 60
#define PERIOD_PING_PRED 5

void stabilize(evutil_socket_t sock, short what, void *arg)
{
	ChordServer *srv = arg;
	static int idx = 0, i;
	Finger *succ, *pred;

	StartLog(DEBUG);
	PartialLog("Stabilizing\n");
	print_server(clog_file_logger()->fp, srv);
	EndLog();
	
	/* While there is no successor, we fix that! */
	if (srv->head_flist == NULL) {
		for (i = 0; i < srv->nknown; i++) {
			send_fs(srv, DEF_TTL, &srv->well_known[i].node.addr,
					srv->well_known[i].node.port, &srv->node.addr,
					srv->node.port);
			send_ping(srv, &srv->well_known[i].node.addr,
					  srv->well_known[i].node.port, get_current_time());
		}
		return;
	}

	/* ping one node in the finger list; these nodes are
	 * pinged in a round robin fashion.
	 * In addition, ping all nodes which have not replied to previous pings
	 */
	ping(srv);

	/* stabilize successor */
	if ((succ = succ_finger(srv)) == NULL)
		return;
	send_stab(srv, &succ->node.addr, succ->node.port, &srv->node.addr,
			  srv->node.port);

	/* ping predecessor. Normally we should hear from our
	 * predecessor via the stabilize message. However, if we
	 * cannot communicate with our true predecessor, our predecessor
	 * will be another node, so we need to ping it...
	 */
	if (idx % PERIOD_PING_PRED == 0) {
		pred = pred_finger(srv);
		assert(pred);
		send_ping(srv, &pred->node.addr, pred->node.port, get_current_time());
	}

	/* fix one backup successor and predessors in a round-robin fashion */
	fix_succs_preds(srv);

	/* fix one proper finger that is not a backup successor;
	 * backup successors are fixed in a round-robin fashion
	 */
	fix_fingers(srv);

	if ((idx++) % CHORD_CLEAN_PERIOD == 0) {
		/* remove all nodes in the finger list that are neither (1)
		 * backup successors, nor (2) proper fingers, and nor (3) backup
		 * predecessors
		 */
		clean_finger_list(srv);
	}

	return;
}

/**********************************************************************/

void fix_fingers(ChordServer *srv)
{
	Finger *f, *succ = succ_finger(srv);
	chordID id = chord_id_successor(srv->node.id, srv->to_fix_finger);
	chordID to_id = chord_id_successor(srv->node.id, NFINGERS-1);

	StartLog(DEBUG);
	print_fun(clog_file_logger()->fp, srv, "fix_finger", &id);
	EndLog();

	/* Only loop across most significant fingers */
	if (id_is_between(&id, &srv->node.id, &succ->node.id)
		|| (srv->to_fix_finger == 0)) {
		/* the problem we are trying to solve here is the one of
		 * loopy graphs, i.e., graphs that are locally consistent
		 * but globally inconsistent (see the Chord TR). Loopy
		 * graphs are quite common in the Internet where the
		 * communication is not necessary symmetric or transitive
		 * (i.e., A can reach B and C, but B cannot reach C).
		 * Loopy graphs cause in lookup failures, as two loops
		 * that originate in different parts of the ring can reach
		 * different targets.
		 * To alleviate loopy graph we ask a random node to resolve
		 * a query to a random id between us and our successor.
		 */
		id_random_between(&srv->node.id, &succ->node.id, &id);
		Node *n = &srv->well_known[random() % srv->nknown].node;
		if (srv->nknown)
			send_fs(srv, DEF_TTL, &n->addr, n->port, &srv->node.addr,
					srv->node.port);
		srv->to_fix_finger = NFINGERS-1;
	}
	else
		srv->to_fix_finger--;

	/* ask one of our fingers to find the proper finger corresponding to id.
	 * preferable this is a far away finger, that share as little routing
	 * information as possible with us
	 */
	if ((f = closest_preceding_finger(srv, &to_id, 0)) == NULL) {
		/* settle for successor... */
		f = succ;
	}

	if (f) {
		send_fs(srv, DEF_TTL, &f->node.addr, f->node.port, &srv->node.addr,
				srv->node.port);

		/* once in a while try to get a better predecessor, as well */
		if (srv->to_fix_finger == NFINGERS-1) {
			if (srv->tail_flist != NULL) {
				id_random_between(&srv->tail_flist->node.id, &srv->node.id, &id);
				send_fs(srv, DEF_TTL, &f->node.addr, f->node.port,
						&srv->node.addr, srv->node.port);
			}
		}
	}
}

/**********************************************************************/
/* fix backup successors and predecessors in a round-robin fashion	  */
/**********************************************************************/

void fix_succs_preds(ChordServer *srv)
{
	int k;
	Finger *f, *succ;
	chordID id;

	StartLog(DEBUG);
	print_fun(clog_file_logger()->fp, srv, "fix_successors", 0);
	EndLog();

	if (succ_finger(srv) == NULL)
		return;

	/* find the next successor to be fixed... */
	for (f = succ_finger(srv), k = 0;
		 k < srv->to_fix_backup && f->next;
		 k++, f = f->next);

	/* ... no more successors to be fixed; restart */
	if (f->next == NULL) {
		srv->to_fix_backup = 0;
		return;
	}

	/* find f's successor */
	id = chord_id_successor(f->node.id, 0);
	send_fs(srv, DEF_TTL, &f->node.addr, f->node.port, &srv->node.addr,
			srv->node.port);
	succ = f;

	/* now fix predecessors; this is not part of the Chord protocol,
	 * but in pactice having more than one predecessor is more robust
	 *
	 * use same index (to_fix_backup) to fix predecessor, as well. Note
	 * that here we assume that NPREDECESSORS <= NSUCCESSORS
	 */
	for (f = pred_finger(srv), k = 0;
		 (k < NPREDECESSORS) && f->prev;
		 k++, f = f->prev) {
		if (f->next == NULL)
			/* f is our known predecessor; if there is a node between our
			 * predecessor and us, we'll get it during the next stabilization
			 * round
			 */
			continue;
		if (f == succ)
			/* f is both a successor and predecessor */
			break;
		if (k == srv->to_fix_backup) {
			/* fix predecessor */
			id_random_between(&f->node.id, &f->next->node.id, &id);
			send_fs(srv, DEF_TTL, &f->node.addr, f->node.port, &srv->node.addr,
					srv->node.port);
			break;
		}
	}

	srv->to_fix_backup++;
	if (srv->to_fix_backup >= NSUCCESSORS)
		srv->to_fix_backup = 0;
}

/************************************************************************/

void ping(ChordServer *srv)
{
	int i;
	Finger *f, *f_next, *f_pinged = NULL;

	/* ping every finger who is still waiting for reply to a previous ping,
	 * and the to_ping-th finger in the list
	 */
	for (f = srv->head_flist, i = 0; f; i++) {
		if (f->npings >= PING_THRESH) {
			char srv_addr[INET_ADDRSTRLEN];
			char dropped_addr[INET_ADDRSTRLEN];

			inet_ntop(AF_INET6, &srv->node.addr, srv_addr, INET6_ADDRSTRLEN);
			inet_ntop(AF_INET6, &f->node.addr, dropped_addr, INET6_ADDRSTRLEN);

			Info("dropping finger[%d] %s:%d (at %s:%d)\n", i, dropped_addr, f->node.port, srv_addr, srv->node.port);

			f_next = f->next;
			remove_finger(srv, f);
		}
		else {
			if (f->npings || (srv->to_ping == i)) {
				f->npings++;
				send_ping(srv, &f->node.addr, f->node.port, get_current_time());
				if (srv->to_ping == i)
					f_pinged = f;
			}
			f_next = f->next;
		}
		f = f_next;
	}

	if (!f_pinged || !(f_pinged->next))
		srv->to_ping = 0;
	else
		srv->to_ping++;
}

/**********************************************************************
 * keep only (1) backup successors, (2) proper fingers, and (3) predecessor;
 * remove anything else from finger list
 ***********************************************************************/

void clean_finger_list(ChordServer *srv)
{
	Finger *f, *f_lastsucc, *f_lastpred, *f_tmp;
	int k;
	chordID id;

	/* skip successor list */
	for (f = srv->head_flist, k = 0;
		 f && (k < NSUCCESSORS-1);
		 f = f->next, k++);
	if (f == NULL || f->next == NULL)
		return;
	f_lastsucc = f;

	/* start from the tail and skip predecessors */
	for (f = srv->tail_flist, k = 0; k < NPREDECESSORS-1; f = f->prev, k++) {
		if (f == f_lastsucc)
			/* finger list contains only of backup successors and predecesor */
			return;
	}
	f_lastpred = f;
	f = f_lastpred->prev;	/* First disposable finger */

	/* keep only unique (proper) fingers */
	for (k = NFINGERS - 1; k >= 0; k--) {
		if (f == f_lastsucc)
			return;

		/* compute srv.id + 2^k */
		id = chord_id_successor(srv->node.id, k);

		if (id_is_between(&id, &f_lastpred->node.id, &srv->node.id)
			|| id_equals(&id, &f_lastpred->node.id)) {
			/* proper finger for id is one of the (backup) predecessors */
			continue;
		}

		if (id_is_between(&id, &srv->node.id, &f_lastsucc->node.id)
			|| id_equals(&id, &srv->node.id))
			/* proper finger for id is one of the (backup) successors */
			break;

		if (id_is_between(&f->node.id, &srv->node.id, &id)) {
			/* f cannot be a proper finger for id, because it
			 * is between current node and id; try next finger
			 */
			continue;
		}

		/* f is a possible proper finger for id */
		while (1) {
			if (f->prev == NULL || f == f_lastsucc)
				return;
			if (id_is_between(&f->prev->node.id, &id, &f->node.id)
				|| id_equals(&f->prev->node.id, &id)) {
				/* this is not a proper finger (i.e., f's predecessor
				 * is between id and f), so remove f
				 */
				f_tmp = f;
				f = f->prev;
				remove_finger(srv, f_tmp);
			}
			else {
				/* f is a proper finger */
				f = f->prev;
				break;
			}
		}
	}
}
