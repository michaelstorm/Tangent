#include <sys/types.h>
#include <netinet/in.h>
#include <openssl/blowfish.h>
#ifdef __APPLE__
#include <inttypes.h>  // Need uint64_t
#endif
#include <stdio.h>
#include "debug.h"

#ifndef CHORD_H
#define CHORD_H

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

enum {
	NBITS        = 160,            /* # bits per ID, same as SHA-1 output */
	ID_LEN       = NBITS/8,        /* bytes per ID */
	TICKET_LEN = 8,			   /* bytes per connection ticket */
	TICKET_TIMEOUT = 32,		   /* seconds for which a ticket is valid */
	ADDRESS_SALTS = 3,			   /* number of IDs an address can have */
	NFINGERS     = NBITS,          /* # fingers per node */
	NSUCCESSORS  = 8,              /* # successors kept */
	NPREDECESSORS = 3,             /* # predecessors kept */
	STABILIZE_PERIOD = 1*1000000,  /* in usec */
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
	DEF_TTL      = 64,             /* default TTL for multi-hop packets */
};

/* packet types */
enum {
	CHORD_ROUTE = 0,   /* data packet */
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
	CHORD_INVALID_ID = -5,
};

/* XXX: warning: portability bugs */
typedef uint8_t byte;
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned long ulong;
#ifdef __APPLE__
typedef u_long ulong;
#endif

typedef struct {
	byte x[ID_LEN];
} chordID;

struct Node
{
	chordID id;
	in_addr_t addr;
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

struct Server
{
	Node node;          /* addr and ID */
	Finger *head_flist; /* head and tail of finger  */
	Finger *tail_flist; /* table + pred + successors */
	int num_passive_fingers;

	int to_fix_finger;  /* next finger to be fixed */
	int to_fix_backup;  /* next successor/predecessor to be fixed */
	int to_ping;        /* next node in finger list to be refreshed */
	uint64_t next_stabilize_us;	/* value of wall_time() at next stabilize */

	int in_sock;      /* incoming socket */
	int out_sock;     /* outgoing socket */

	Node well_known[MAX_WELLKNOWN];
	int nknown;

	chordID key_array[MAX_KEY_NUM];
	int num_keys;

	BF_KEY ticket_key;
};

typedef struct
{
	ulong addr;
	ushort port;
} host;

#define PRED(srv)  (srv->tail_flist)
#define SUCC(srv)  (srv->head_flist)

/* the keys in key_array are read from file acclist.txt,
 * and are used to authenticate users sending control
 * messages such as CHORD_FINGERS_GET.
 * This mechanism is intended to prevent trivial DDoS attacks.
 *
 * (For now the keyes are sent and stored in clear so
 *  the security provided by this mechanism is quite weak)
 */
#define ACCLIST_FILE "acclist.txt"

/* chord.c */
void chord_main(char *conf_file, int parent_sock);
void initialize(Server *srv);
void handle_packet(int network);
int read_keys(char *file, chordID *keyarray, int max_num_keys);

/* finger.c */
Finger *new_finger(Node *node);
Finger *succ_finger(Server *srv);
Finger *pred_finger(Server *srv);
Finger *closest_preceding_finger(Server *srv, chordID *id, int fall);
Node *closest_preceding_node(Server *srv, chordID *id, int fall);
void remove_finger(Server *srv, Finger *f);
Finger *get_finger(Server *srv, chordID *id);
Finger *insert_finger(Server *srv, chordID *id, in_addr_t addr, in_port_t port,
					  int *fnew);
void free_finger_list(Finger *flist);

/* hosts.c */
in_addr_t get_addr();

/* join.c */
void join(Server *srv, FILE *fp);

/* pack.c */
int dispatch(Server *srv, int n, uchar *buf, host *from);
int pack(uchar *buf, char *fmt, ...);
int unpack(uchar *buf, char *fmt, ...);
int sizeof_fmt(char *fmt);

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
			  uchar *data);
int unpack_data(Server *srv, int n, uchar *buf, host *from);
int pack_fs(uchar *buf, uchar *ticket, byte ttl, chordID *id, ulong addr,
			ushort port);
int unpack_fs(Server *srv, int n, uchar *buf, host *from);
int pack_fs_repl(uchar *buf, uchar *ticket, chordID *id, ulong addr,
				 ushort port);
int unpack_fs_repl(Server *srv, int n, uchar *buf, host *from);
int pack_stab(uchar *buf, chordID *id, ulong addr, ushort port);
int unpack_stab(Server *srv, int n, uchar *buf, host *from);
int pack_stab_repl(uchar *buf, chordID *id, ulong addr, ushort port);
int unpack_stab_repl(Server *srv, int n, uchar *buf, host *from);
int pack_notify(uchar *buf, chordID *id, ulong addr, ushort port);
int unpack_notify(Server *srv, int n, uchar *buf, host *from);
int pack_ping(uchar *buf, uchar *ticket, chordID *id, ulong addr, ushort port,
			  ulong time);
int unpack_ping(Server *srv, int n, uchar *buf, host *from);
int pack_pong(uchar *buf, uchar *ticket, chordID *id, ulong addr, ushort port,
			  ulong time);
int unpack_pong(Server *srv, int n, uchar *buf, host *from);
int pack_fingers_get(uchar *buf, uchar *ticket, ulong addr, ushort port,
					 chordID *key);
int unpack_fingers_get(Server *srv, int n, uchar *buf, host *from);
int pack_fingers_repl(uchar *buf, Server *srv, uchar *ticket);
int unpack_fingers_repl(Server *null, int n, uchar *buf, host *from);
int pack_traceroute(uchar *buf, Server *srv, Finger *f, uchar type, byte ttl,
					byte hops);
int unpack_traceroute(Server *srv, int n, uchar *buf, host *from);
int pack_traceroute_repl(uchar *buf, Server *srv, byte ttl, byte hops,
						 ulong *paddr, ushort *pport, int one_hop);
int unpack_traceroute_repl(Server *srv, int n, uchar *buf, host *from);

/* process.c */
int process_data(Server *srv, uchar type, byte ttl, chordID *id, ushort len,
				 uchar *data);
int process_fs(Server *srv, uchar *ticket, byte ttl, chordID *id, ulong addr,
			   ushort port);
int process_fs_repl(Server *srv, uchar *ticket, chordID *id, ulong addr,
					ushort port);
int process_stab(Server *srv, chordID *id, ulong addr, ushort port);
int process_stab_repl(Server *srv, chordID *id, ulong addr, ushort port);
int process_notify(Server *srv, chordID *id, ulong addr, ushort port);
int process_ping(Server *srv, uchar *ticket, chordID *id, ulong addr,
				 ushort port, ulong time);
int process_pong(Server *srv, uchar *ticket, chordID *id, ulong addr,
				 ushort port, ulong time, host *from);
int process_fingers_get(Server *srv, uchar *ticket, ulong addr, ushort port,
						chordID *key);
int process_fingers_repl(Server *srv, uchar ret_code);
int process_traceroute(Server *srv, chordID *id, char *buf, uchar type,
					   byte ttl, byte hops);
int process_traceroute_repl(Server *srv, char *buf, byte ttl, byte hops);

/* sendpkt.c */
void send_raw(Server *srv, in_addr_t addr, in_port_t port, int n, uchar *buf);
void send_data(Server *srv, uchar type, byte ttl, Node *np, chordID *id,
			   ushort n, uchar *data);
void send_fs(Server *srv, byte ttl, ulong to_addr, ushort to_port, chordID *id,
			 ulong addr, ushort port);
void send_fs_forward(Server *srv, uchar *ticket, byte ttl, ulong to_addr,
			 ushort to_port, chordID *id, ulong addr, ushort port);
void send_fs_repl(Server *srv, uchar *ticket, ulong to_addr, ushort to_port,
				  chordID *id, ulong addr, ushort port);
void send_stab(Server *srv, ulong to_addr, ushort to_port, chordID *id,
			   ulong addr, ushort port);
void send_stab_repl(Server *srv, ulong to_addr, ushort to_port, chordID *id,
					ulong addr, ushort port);
void send_notify(Server *srv, ulong to_addr, ushort to_port, chordID *id,
				 ulong addr, ushort port);
void send_ping(Server *srv, ulong to_addr, ushort to_port, ulong addr,
			   ushort port, ulong time);
void send_pong(Server *srv, uchar *ticket, ulong to_addr, ushort to_port,
			   ulong time);
void send_fingers_get(Server *srv, ulong to_addr, ushort to_port, ulong addr,
					  ushort port, chordID *key);
void send_fingers_repl(Server *srv, uchar *ticket, ulong to_addr,
					   ushort to_port);
void send_traceroute(Server *srv, Finger *f, uchar *buf, uchar type, byte ttl,
					 byte hops);
void send_traceroute_repl(Server *srv, uchar *buf, int ttl, int hops,
						  int one_hop);

/* stabilize.c */
void stabilize(Server *srv);
void set_stabilize_timer(Server *srv);

/* api.c */
int chord_init(char *conf_file);
void chord_cleanup(int signum);
void chord_route(chordID *k, char *data, int len);
void chord_deliver(int n, uchar *data);
void chord_get_range(chordID *l, chordID *r);
void chord_update_range(chordID *l, chordID *r);
int chord_is_local(chordID *x);

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
void print_process(Server *srv, char *process_type, chordID *id, ulong addr,
				   ushort port);
void print_send(Server *srv, char *send_type, chordID *id, ulong addr,
				ushort port);
void print_fun(Server *srv, char *fun_name, chordID *id);
void print_current_time(char *prefix, char *suffix);
int match_key(chordID *key_array, int num_keys, chordID *key);

int pack_ticket(BF_KEY *key, uchar *out, char *fmt, ...);
int verify_ticket(BF_KEY *key, uchar *ticket_enc, char *fmt, ...);

// undefine this when not going over loopback, since this could allow a
// malicious peer to assign ~65000 IDs to itself
#define HASH_PORT_WITH_ADDRESS
#ifdef HASH_PORT_WITH_ADDRESS
void get_address_id(chordID *id, ulong addr, ushort port);
int verify_address_id(chordID *id, ulong addr, ushort port);
#else
void get_address_id(chordID *id, ulong addr);
int verify_address_id(chordID *id, ulong addr);
#endif

int encrypt(const uchar *clear, int len, uchar *encrypted, uchar *key,
			uchar *iv);
int decrypt(const uchar *encrypted, int len, uchar *clear, uchar *key,
			uchar *iv);

#include "eprintf.h"

#endif /* INCL_CHORD_H */
