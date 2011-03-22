#ifndef CHORD_H
#define CHORD_H

#include <event2/event.h>
#include <netinet/in.h>
#include <openssl/blowfish.h>
#include <sys/types.h>
#ifdef __APPLE__
#include <inttypes.h>  // Need uint64_t
#endif
#include <stdio.h>
#include "chord_api.h"
#include "debug.h"
#include "eprintf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Finger Finger;
typedef struct Node Node;
typedef struct Server Server;

#define NELEMS(a) (sizeof(a) / sizeof(a[0]))
#ifndef FALSE
#define FALSE 0
#define TRUE  1
#endif

/* whether a finger is passive or active */
#define F_PASSIVE 0
#define F_ACTIVE  1

#define V4_MAPPED(x) IN6_IS_ADDR_V4MAPPED(x)

enum {
	TICKET_LEN = 8,				   /* bytes per connection ticket */
	TICKET_TIMEOUT = 8,		   /* seconds for which a ticket is valid */
	ADDRESS_SALTS = 3,			   /* number of IDs an address can have */
	NFINGERS     = CHORD_ID_BITS,  /* # fingers per node */
	NSUCCESSORS  = 8,              /* # successors kept */
	NPREDECESSORS = 3,             /* # predecessors kept */
	ADDR_DISCOVER_INTERVAL = 1*1000000,
	STABILIZE_PERIOD = 10*1000000,  /* in usec */
	BUFSIZE      = 65535,          /* buffer for packets */
	MAX_WELLKNOWN = 50,            /* maximum number of other known servers
									*  (read from configuration file)
									*/
	MAX_SIMJOIN = 4,               /* maximum number of servers
									* contacted simultaneously when joining
									*/
	MAX_PASSIVE_FINGERS = 20,	   /* maximum number of fingers to keep that
									* have yet to respond to ping
									*/
	PING_THRESH = 5,               /* this many unanswered pings are allowed */
	DEF_TTL      = 32,             /* default TTL for multi-hop packets */
};

/* packet types */
enum {
	CHORD_ADDR_DISCOVER = 0,
	CHORD_ADDR_DISCOVER_REPL,
	CHORD_ROUTE,   /* data packet */
	CHORD_ROUTE_LAST,
	CHORD_FS,          /* find_successor */
	CHORD_FS_REPL,     /* find_successor reply */
	CHORD_STAB,        /* get predecessor */
	CHORD_STAB_REPL,   /* ... response */
	CHORD_NOTIFY,      /* notify (predecessor) */
	CHORD_PING,        /* are you alive? */
	CHORD_PONG,        /* yes, I am */
	CHORD_FINGERS_GET, /* get your finger list */
	CHORD_FINGERS_REPL,/* .. here is my finger list */
	CHORD_TRACEROUTE,  /* traceroute */
	CHORD_TRACEROUTE_LAST,
	CHORD_TRACEROUTE_REPL,/* traceroute repl */
};

enum {
	CHORD_PROTOCOL_ERROR = -1,
	CHORD_TTL_EXPIRED = -2,
	CHORD_INVALID_TICKET = -3,
	CHORD_PACK_ERROR = -4,
	CHORD_ADDR_UNDISCOVERED = -5,
};

enum {
	CHORD_EVENT_UPDATE_RANGE = 0,
};

/* XXX: warning: portability bugs */
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef struct in6_addr in6_addr;
typedef struct in_addr in_addr;
#ifdef __APPLE__
typedef u_long ulong;
#endif

typedef int (*chord_packet_handler)(void *ctx, Server *srv, int n, uchar *buf,
									Node *from);

struct Node
{
	chordID id;
	in6_addr addr;
	in_port_t port;
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

/* Finger table contains NFINGERS fingers, then predecessor, then
   the successor list */

#define MAX_KEY_NUM 20

struct WellKnown
{
	Node node;
	in6_addr reflect_addr;
};

struct Server
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

	int tunnel_sock;

	struct WellKnown well_known[MAX_WELLKNOWN];
	int nknown;

	chordID key_array[MAX_KEY_NUM];
	int num_keys;

	struct event_base *ev_base;
	struct event *sock_event;
	struct event *stab_event;
	struct event *discover_addr_event;

	BF_KEY ticket_key;

	chord_packet_handler packet_handlers[CHORD_TRACEROUTE_REPL];
	void *packet_handler_ctx;
};

#define PRED(srv) (srv->tail_flist)
#define SUCC(srv) (srv->head_flist)

/* chord.c */
Server *new_server(struct event_base *ev_base, int tunnel_sock);
void server_initialize_from_file(Server *srv, char *conf_file);
void server_start(Server *srv);
void server_initialize_socket(Server *srv);
void set_socket_nonblocking(int sock);

void chord_main(char **conf_files, int nservers, int tunnel_sock);
void initialize(Server *srv, int is_v6);
void handle_packet(evutil_socket_t sock, short what, void *arg);
int read_keys(char *file, chordID *keyarray, int max_num_keys);

void chord_update_range(Server *srv, chordID *l, chordID *r);
void chord_get_range(Server *srv, chordID *l, chordID *r);
int chord_is_local(Server *srv, chordID *x);

void chord_set_packet_handler(Server *srv, int event,
							  chord_packet_handler handler);
void chord_set_packet_handler_ctx(Server *srv, void *ctx);

/* finger.c */
Finger *new_finger(Node *node);
Finger *succ_finger(Server *srv);
Finger *pred_finger(Server *srv);
Finger *closest_preceding_finger(Server *srv, chordID *id, int fall);
Node *closest_preceding_node(Server *srv, chordID *id, int fall);
void remove_finger(Server *srv, Finger *f);
Finger *get_finger(Server *srv, chordID *id);
Finger *insert_finger(Server *srv, chordID *id, in6_addr *addr, in_port_t port,
					  int *fnew);
void free_finger_list(Finger *flist);

/* hosts.c */
in_addr_t get_addr();
void to_v6addr(ulong v4addr, in6_addr *v6addr);
ulong to_v4addr(const in6_addr *v6addr);
char *v6addr_to_str(const in6_addr *v6addr);
int resolve_v6name(const char *name, in6_addr *v6addr);
void chord_bind_v6socket(int sock, const in6_addr *addr, ushort port);
void chord_bind_v4socket(int sock, ulong addr, ushort port);

/* join.c */
void discover_addr(evutil_socket_t sock, short what, void *arg);
void join(Server *srv, FILE *fp);

/* pack.c */
int dispatch(Server *srv, int n, uchar *buf, Node *from);
int pack(uchar *buf, const char *fmt, ...);
int unpack(uchar *buf, const char *fmt, ...);
int sizeof_fmt(const char *fmt);

#ifdef CCURED
// These are the kinds of arguments that we pass to pack
struct pack_args {
  int f1;
  chordID * f2;
};
#pragma ccuredvararg("pack", sizeof(struct pack_args))
struct unpack_args {
  ushort * f1;
  uchar * f2;
  ulong * f3;
  chordID *id;
};
#pragma ccuredvararg("unpack", sizeof(struct unpack_args))
#endif

int pack_data(uchar *buf, uchar type, byte ttl, chordID *id, ushort len,
			  const uchar *data);
int unpack_data(Server *srv, int n, uchar *buf, Node *from);
int pack_fs(uchar *buf, uchar *ticket, byte ttl, in6_addr *addr, ushort port);
int unpack_fs(Server *srv, int n, uchar *buf, Node *from);
int pack_fs_repl(uchar *buf, uchar *ticket, in6_addr *addr, ushort port);
int unpack_fs_repl(Server *srv, int n, uchar *buf, Node *from);
int pack_stab(uchar *buf, in6_addr *addr, ushort port);
int unpack_stab(Server *srv, int n, uchar *buf, Node *from);
int pack_stab_repl(uchar *buf, in6_addr *addr, ushort port);
int unpack_stab_repl(Server *srv, int n, uchar *buf, Node *from);
int pack_notify(uchar *buf);
int unpack_notify(Server *srv, int n, uchar *buf, Node *from);
int pack_ping(uchar *buf, uchar *ticket, ulong time);
int unpack_ping(Server *srv, int n, uchar *buf, Node *from);
int pack_pong(uchar *buf, uchar *ticket, ulong time);
int unpack_pong(Server *srv, int n, uchar *buf, Node *from);
int pack_fingers_get(uchar *buf, uchar *ticket, in6_addr *addr, ushort port,
					 chordID *key);
int unpack_fingers_get(Server *srv, int n, uchar *buf, Node *from);
int pack_fingers_repl(uchar *buf, Server *srv, uchar *ticket);
int unpack_fingers_repl(Server *null, int n, uchar *buf, Node *from);
int pack_traceroute(uchar *buf, Server *srv, Finger *f, uchar type, byte ttl,
					byte hops);
int unpack_traceroute(Server *srv, int n, uchar *buf, Node *from);
int pack_traceroute_repl(uchar *buf, Server *srv, byte ttl, byte hops,
						 in6_addr *paddr, ushort *pport, int one_hop);
int unpack_traceroute_repl(Server *srv, int n, uchar *buf, Node *from);
int pack_addr_discover(uchar *buf, uchar *ticket);
int unpack_addr_discover(Server *srv, int n, uchar *buf, Node *from);
int pack_addr_discover_repl(uchar *buf, uchar *ticket, in6_addr *addr);
int unpack_addr_discover_repl(Server *srv, int n, uchar *buf, Node *from);

/* process.c */
Node *next_route_node(Server *srv, chordID *id, uchar pkt_type,
					  uchar *route_type);
int process_data(Server *srv, uchar type, byte ttl, chordID *id, ushort len,
				 uchar *data, Node *from);
int process_fs(Server *srv, uchar *ticket, byte ttl, in6_addr *addr,
			   ushort port);
int process_fs_repl(Server *srv, uchar *ticket, in6_addr *addr, ushort port);
int process_stab(Server *srv, in6_addr *addr, ushort port);
int process_stab_repl(Server *srv, in6_addr *addr, ushort port);
int process_notify(Server *srv, Node *from);
int process_ping(Server *srv, uchar *ticket, ulong time, Node *from);
int process_pong(Server *srv, uchar *ticket, ulong time, Node *from);
int process_fingers_get(Server *srv, uchar *ticket, in6_addr *addr, ushort port,
						chordID *key);
int process_fingers_repl(Server *srv, uchar ret_code);
int process_traceroute(Server *srv, chordID *id, char *buf, uchar type,
					   byte ttl, byte hops);
int process_traceroute_repl(Server *srv, char *buf, byte ttl, byte hops);
int process_addr_discover(Server *srv, uchar *ticket, Node *from);
int process_addr_discover_repl(Server *srv, uchar *ticket, in6_addr *addr,
							   Node *from);

/* sendpkt.c */
void send_packet(Server *srv, in6_addr *addr, in_port_t port, int n,
				 uchar *buf);
void send_raw_v4(int sock, in6_addr *addr, in_port_t port, int n, uchar *buf);
void send_raw_v6(int sock, in6_addr *addr, in_port_t port, int n, uchar *buf);
void send_data(Server *srv, uchar type, byte ttl, Node *np, chordID *id,
			   ushort n, const uchar *data);
void send_fs(Server *srv, byte ttl, in6_addr *to_addr, ushort to_port,
			 in6_addr *addr, ushort port);
void send_fs_forward(Server *srv, uchar *ticket, byte ttl, in6_addr *to_addr,
					 ushort to_port, in6_addr *addr, ushort port);
void send_fs_repl(Server *srv, uchar *ticket, in6_addr *to_addr, ushort to_port,
				  in6_addr *addr, ushort port);
void send_stab(Server *srv, in6_addr *to_addr, ushort to_port, in6_addr *addr,
			   ushort port);
void send_stab_repl(Server *srv, in6_addr *to_addr, ushort to_port,
					in6_addr *addr, ushort port);
void send_notify(Server *srv, in6_addr *to_addr, ushort to_port);
void send_ping(Server *srv, in6_addr *to_addr, ushort to_port, ulong time);
void send_pong(Server *srv, uchar *ticket, in6_addr *to_addr, ushort to_port,
			   ulong time);
void send_fingers_get(Server *srv, in6_addr *to_addr, ushort to_port, in6_addr *addr,
					  ushort port, chordID *key);
void send_fingers_repl(Server *srv, uchar *ticket, in6_addr *to_addr,
					   ushort to_port);
void send_traceroute(Server *srv, Finger *f, uchar *buf, uchar type, byte ttl,
					 byte hops);
void send_traceroute_repl(Server *srv, uchar *buf, int ttl, int hops,
						  int one_hop);
void send_addr_discover(Server *srv, in6_addr *to_addr, ushort to_port);
void send_addr_discover_repl(Server *srv, uchar *ticket, in6_addr *to_addr,
							 ushort to_port);

/* stabilize.c */
void stabilize(evutil_socket_t sock, short what, void *arg);

/* util.c */
double f_rand();
double funif_rand(double a, double b);
int n_rand(int n);
int unif_rand(int a, int b);
uint64_t wall_time();
ulong get_current_time();
void update_rtt(long *rtt_avg, long *rtt_std, long new_rtt);
chordID rand_ID();
chordID successor(chordID id, int n);
chordID predecessor(chordID id, int n);
void add(chordID *a, chordID *b, chordID *res);
void subtract(chordID *a, chordID *b, chordID *res);
void random_between(chordID *a, chordID *b, chordID *res);
int msb(chordID *x);
int equals(chordID *a, chordID *b);
int equals_id_str(chordID *a, char *b);
int is_zero(chordID *x);
int is_between(chordID *x, chordID *a, chordID *b);
int copy_id( chordID *a, chordID *b);
void print_id(FILE *f, chordID *id);
chordID atoid(const char *str);
unsigned hash(chordID *id, unsigned n);
void print_chordID(chordID *id);
void print_two_chordIDs(char *preffix, chordID *id1,
						char *middle, chordID *id2,
						char *suffix);
void print_three_chordIDs(char *preffix, chordID *id1,
						  char *middle1, chordID *id2,
						  char *middle2, chordID *id3,
						  char *suffix);
void print_node(Node *node, char *prefix, char *suffix);
void print_finger(Finger *f, char *prefix, char *suffix);
void print_finger_list(Finger *fhead, char *prefix, char *suffix);
void print_server(Server *s, char *prefix, char *suffix);
void print_process(Server *srv, char *process_type, chordID *id, in6_addr *addr,
				   ushort port);
void print_send(Server *srv, char *send_type, chordID *id, in6_addr *addr,
				ushort port);
void print_fun(Server *srv, char *fun_name, chordID *id);
void print_current_time(char *prefix, char *suffix);
int match_key(chordID *key_array, int num_keys, chordID *key);
int v6_addr_equals(const in6_addr *addr1, const in6_addr *addr2);
void v6_addr_copy(in6_addr *dest, const in6_addr *src);

int pack_ticket(BF_KEY *key, const uchar *out, const char *fmt, ...);
int verify_ticket(BF_KEY *key, const uchar *ticket_enc, const char *fmt, ...);

void get_data_id(chordID *id, const uchar *buf, int n);
void get_address_id(chordID *id, in6_addr *addr, ushort port);
int verify_address_id(chordID *id, in6_addr *addr, ushort port);

#ifdef __cplusplus
}
#endif

#endif /* INCL_CHORD_H */
