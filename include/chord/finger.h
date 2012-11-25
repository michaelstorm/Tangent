#ifndef FINGER_H
#define FINGER_H

#ifdef __cplusplus
extern "C" {
#endif

struct FingerList
{
	Finger *head;
	Finger *tail;
};

struct Finger
{
	Node node;          /* ID and address of finger */
	int status;         /* specifies whether this finger has been
						 * pinged; possible values: F_PASSIVE (the node
						 * has not been pinged) and F_ACTIVE (the node
						 * has been pinged)
						 */
	int npings;         /* # of unanswered pings */
	long rtt_avg;       /* average rtt to finger (usec) */
	long rtt_dev;       /* rtt's mean deviation (usec) */
						/* rtt_avg, rtt_dev can be used to implement
						 * proximity routing or set up RTO for ping
						 */
	Finger *next;
	Finger *prev;
};

Finger *new_finger(Node *node);
Finger *succ_finger(Server *srv);
Finger *pred_finger(Server *srv);
Finger *closest_preceding_finger(Server *srv, chordID *id, int fall);
Node *closest_preceding_node(Server *srv, chordID *id, int fall);
void remove_finger(Server *srv, Finger *f);
Finger *get_finger(Server *srv, chordID *id, int *index);
Finger *insert_finger(Server *srv, chordID *id, in6_addr *addr, in_port_t port,
					  int *fnew);
void activate_finger(Server *srv, Finger *f);
void free_finger_list(Finger *flist);

#ifdef __cplusplus
}
#endif

#endif