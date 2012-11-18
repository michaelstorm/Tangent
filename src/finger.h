#ifndef FINGER_H
#define FINGER_H

#ifdef __cplusplus
extern "C" {
#endif

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