#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "chord.h"

/* new_finger: allocate and initialize finger structure */
Finger *new_finger(Node *node)
{
	Finger *fp;

	/* allocate space for new finger */
	fp = emalloc(sizeof(Finger));
	fp->node = *node;
	fp->status = F_PASSIVE;
	fp->npings = 0;
	fp->next = fp->prev = NULL;
	fp->rtt_avg = fp->rtt_dev = 0;
	return fp;
}

/**********************************************************************/

Finger *succ_finger(Server *srv)
{
	Finger *f;
	for (f = srv->head_flist; f; f = f->next) {
		if (f->status == F_ACTIVE)
			return f;
	}
	return NULL;
}

/**********************************************************************/

Finger *pred_finger(Server *srv)
{
	Finger *f;
	for (f = srv->tail_flist; f; f = f->prev) {
		if (f->status == F_ACTIVE)
			return f;
	}
	return NULL;
}

/**********************************************************************/

/* closest_preceding_node: search table for highest predecessor of id */
/* closest_preceding_finger: search table for highest predecessor of id */

Finger *closest_preceding_finger(Server *srv, chordID *id, int fall)
{
	Finger *f;
	for (f = srv->tail_flist; f; f = f->prev) {
		/* look only for active fingers; we do not know if we can
		 * reach the passive fingers
		 */
		if ((fall == TRUE) || (f->status == F_ACTIVE)) {
			if (is_between(&f->node.id, &srv->node.id, id))
				return f;
		}
	}

	return NULL;
}

Node *closest_preceding_node(Server *srv, chordID *id, int fall)
{
	Finger *f = closest_preceding_finger(srv, id, fall);

	if (f == NULL)
		return &srv->node;
	else
		return &f->node;
}

/**********************************************************************/

Finger *get_finger(Server *srv, chordID *id)
{
	Finger *f;
	for (f = srv->head_flist; f; f = f->next) {
		if (equals(id, &f->node.id))
			return f;
	}
	return NULL;
}

/* find the finger that has not responded to the greatest number of pings */
Finger *get_worst_passive_finger(Server *srv)
{
	Finger *f;
	Finger *worst_finger = NULL;
	int npings_record = -1;

	for (f = srv->head_flist; f; f = f->next) {
		if (f->status == F_PASSIVE && f->npings > npings_record) {
			npings_record = f->npings;
			worst_finger = f;

			// stop searching if we've found the worst we can get
			if (npings_record >= PING_THRESH-1)
				return worst_finger;
		}
	}
	return worst_finger;
}

/**********************************************************************/

Finger *insert_finger(Server *srv, chordID *id, in6_addr *addr, in_port_t port,
					  int *fnew)
{
	Finger *f, *new_f, *pred;
	Node n;

	if (memcmp(srv->node.addr.s6_addr, addr->s6_addr, 16) == 0 && srv->node.port == port) {
		weprintf("dropping finger request from ourself");
		return NULL;
	}

	pred = PRED(srv);
	f = get_finger(srv, id);

	if (f) {
		if (memcmp(f->node.addr.s6_addr, addr->s6_addr, 16) != 0) {
			memcpy(f->node.addr.s6_addr, addr->s6_addr, 16);
			f->node.port = port;
			f->rtt_avg = f->rtt_dev = 0;
			f->npings = 0;
		}

		/* f is already in the list. In this case,
		 * f is not refreshed, i.e., f->npings is not set to 0.
		 * Refreshing f here might preclude the ping procedure from removing
		 * f when it dies.
		 */
		CHORD_DEBUG(5, print_server(srv, "[insert_finger(1)]", "end"));
		*fnew = FALSE;
		return f;
	}

	// only hold onto a certain number of passive fingers, to avoid getting
	// DoS'd
	if (srv->num_passive_fingers >= MAX_PASSIVE_FINGERS) {
		Finger *worst_finger = get_worst_passive_finger(srv);
		assert(worst_finger);
		remove_finger(srv, worst_finger);
	}

	srv->num_passive_fingers++;

	n.id = *id; memcpy(n.addr.s6_addr, addr, 16); n.port = port;
	new_f = new_finger(&n);

	f = srv->head_flist;
	if (f == NULL) {
		/* this is the first finger */
		srv->head_flist = srv->tail_flist = new_f;
	}
	else {
		f = closest_preceding_finger(srv, id, TRUE);
		if (f == NULL) {
			new_f->next = srv->head_flist;
			new_f->prev = NULL;
			srv->head_flist->prev = new_f;
			srv->head_flist = new_f;
		}
		else {
			new_f->next = f->next;
			if (f->next)
				f->next->prev = new_f;
			else
				srv->tail_flist = new_f;
			new_f->prev = f;
			f->next = new_f;
		}
	}

	CHORD_DEBUG(5, print_server(srv, "[insert_finger(2)]", ""));

	*fnew = TRUE;
	return new_f;
}

void activate_finger(Server *srv, Finger *f)
{
	srv->num_passive_fingers--;
	f->status = F_ACTIVE;
}

/**********************************************************************/

void remove_finger(Server *srv, Finger *f)
{
	Finger *pred, *pf;

	if (f->status == F_PASSIVE)
		srv->num_passive_fingers--;

	pred = pred_finger(srv); /* remember to check whether pred changes */

	if ((srv->tail_flist != f) && (srv->head_flist != f)) {
		f->prev->next = f->next;
		f->next->prev = f->prev;
	}
	else {
		if (srv->head_flist == f) {
			srv->head_flist = f->next;
			if (f->next)
				f->next->prev = NULL;
		}
		if (srv->tail_flist == f) {
			srv->tail_flist = f->prev;
			if (f->prev)
				f->prev->next = NULL;
		}
	}

	pf = pred_finger(srv);
	if (pred != pf) {
		if (pf == NULL)
			chord_update_range(srv, &srv->node.id, &srv->node.id);
		else
			chord_update_range(srv, &pf->node.id, &srv->node.id);
	}

	CHORD_DEBUG(5, print_server(srv, "[remove_finger]", ""));
	free(f);
}

/**********************************************************************/

void free_finger_list(Finger *flist)
{
	Finger *f;

	while (flist) {
		f = flist;
		flist = flist->next;
		free(f);
	}
}
