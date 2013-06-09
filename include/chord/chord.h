#ifndef CHORD_H
#define CHORD_H

#include <event2/event.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "chord/chord_api.h"
#include "chord/chord_opts.h"
#include "chord/eprintf.h"
#include "chord/logger/clog.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __APPLE__
typedef u_long ulong;
#endif

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef struct in6_addr in6_addr;
typedef struct in_addr in_addr;
typedef struct Finger Finger;
typedef struct Node Node;
typedef struct ChordServer ChordServer;

#define NELEMS(a) (sizeof(a) / sizeof(a[0]))

/* whether a finger is passive or active */
#define F_PASSIVE 0
#define F_ACTIVE  1

#define V4_MAPPED(x) IN6_IS_ADDR_V4MAPPED(x)

/* packet types */
enum {
	CHORD_ADDR_DISCOVER = 0,
	CHORD_ADDR_DISCOVER_REPLY,
	CHORD_DATA,   /* data packet */
	CHORD_FS,          /* find_successor */
	CHORD_FS_REPLY,     /* find_successor reply */
	CHORD_STAB,        /* get predecessor */
	CHORD_STAB_REPLY,   /* ... response */
	CHORD_NOTIFY,      /* notify (predecessor) */
	CHORD_PING,        /* are you alive? */
	CHORD_PONG,        /* yes, I am */
};

extern const char *PACKET_NAMES[];
extern int MAX_PACKET_TYPE;

enum {
	CHORD_NO_ERROR = 0,
	CHORD_PROTOCOL_ERROR,
	CHORD_TTL_EXPIRED,
	CHORD_INVALID_TICKET,
	CHORD_ADDR_UNDISCOVERED,
	CHORD_SELF_ORIGINATOR,
	CHORD_FINGER_ERROR,
};

typedef int (*chord_packet_handler)(void *ctx, ChordServer *srv, void *msg,
									Node *from);

struct Node
{
	chordID id;
	in6_addr addr;
	in_port_t port;
	struct Node *next;
};

/* Finger table contains NFINGERS fingers, then predecessor, then
   the successor list */

#define MAX_KEY_NUM 20

typedef void (*send_func_t)(int sock, in6_addr *addr, in_port_t port, int n, uchar *buf);

struct ChordServerElement
{
	struct ChordServer *value;
	struct ChordServerElement *next;
};

struct ChordServer
{
	Node node;          /* addr and ID */
	chordID pred_bound; /* left bound on ID range; right bound is node.id */
	Finger *head_flist; /* head and tail of finger  */
	Finger *tail_flist; /* table + pred + successors */
	int num_passive_fingers;

	int to_fix_finger;  /* next finger to be fixed */
	int to_fix_backup;  /* next successor/predecessor to be fixed */
	int to_ping;        /* next node in finger list to be refreshed */

	int sock;        /* incoming/outgoing socket */
	int is_v6;		 /* whether we're sitting on an IPv6 interface */
	send_func_t send_func;

	int tunnel_sock;

	struct Node *well_known;

	chordID key_array[MAX_KEY_NUM];
	int num_keys;

	struct event_base *ev_base;
	struct event *sock_event;
	struct event *stab_event;
	struct event *discover_addr_event;

	uchar *ticket_salt;
	int ticket_salt_len;
	int ticket_hash_len;

	chord_packet_handler packet_handlers[CHORD_PONG+1];
	void *packet_handler_ctx;

	struct Dispatcher *dispatcher;
};

/* chord.c */
int chord_check_library_versions() DLL_PUBLIC;
ChordServer *new_server(struct event_base *ev_base) DLL_PUBLIC;
struct ChordServerElement *server_initialize_list_from_file(struct event_base *ev_base, char *conf_file) DLL_PUBLIC;
void server_start(ChordServer *srv) DLL_PUBLIC;
void server_initialize_socket(ChordServer *srv) DLL_PUBLIC;

void handle_packet(evutil_socket_t sock, short what, void *arg);

void chord_update_range(ChordServer *srv, chordID *l, chordID *r);

/* join.c */
void discover_addr(evutil_socket_t sock, short what, void *arg);

/* stabilize.c */
void stabilize(evutil_socket_t sock, short what, void *arg);

#ifdef __cplusplus
}
#endif

#endif
