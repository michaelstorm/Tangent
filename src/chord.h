#ifndef CHORD_H
#define CHORD_H

#include <event2/event.h>
#include <netinet/in.h>
#include <sys/types.h>
#ifdef __APPLE__
#include <inttypes.h>  // Need uint64_t
#endif
#include <stdio.h>
#include <stdlib.h>
#include "chord_api.h"
#include "debug.h"
#include "eprintf.h"
#include "messages.pb-c.h"
#include "logger/logger.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Finger Finger;
typedef struct Node Node;
typedef struct Server Server;
typedef struct LinkedStringNode LinkedStringNode;
typedef struct LinkedString LinkedString;

#define NELEMS(a) (sizeof(a) / sizeof(a[0]))
#ifndef FALSE
#define FALSE 0
#define TRUE  1
#endif

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_TRACE
#endif

/* whether a finger is passive or active */
#define F_PASSIVE 0
#define F_ACTIVE  1

#define V4_MAPPED(x) IN6_IS_ADDR_V4MAPPED(x)

enum {
	CHORD_WIRE_VERSION = 1,
	TICKET_TIMEOUT = 8,		   /* seconds for which a ticket is valid */
	TICKET_HASH_LEN = 4,
	TICKET_SALT_LEN = 16,
	ADDRESS_SALTS = 3,			   /* number of IDs an address can have */
	NFINGERS     = CHORD_ID_BITS,  /* # fingers per node */
	NSUCCESSORS  = 8,              /* # successors kept */
	NPREDECESSORS = 3,             /* # predecessors kept */
	ADDR_DISCOVER_INTERVAL = 1*1000000,
	STABILIZE_PERIOD = 3*1000000,  /* in usec */
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

enum {
	CHORD_NO_ERROR = 0,
	CHORD_PROTOCOL_ERROR,
	CHORD_TTL_EXPIRED,
	CHORD_INVALID_TICKET,
	CHORD_ADDR_UNDISCOVERED,
	CHORD_SELF_ORIGINATOR,
	CHORD_FINGER_ERROR,
};

typedef int (*chord_packet_handler)(void *ctx, Server *srv, void *msg,
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

	uchar *ticket_salt;
	int ticket_salt_len;
	int ticket_hash_len;

	chord_packet_handler packet_handlers[CHORD_PONG+1];
	void *packet_handler_ctx;

	struct Dispatcher *dispatcher;
};

struct LinkedStringNode {
	const char *chars;
	int len;
	LinkedStringNode *next;
};

struct LinkedString {
	LinkedStringNode *first;
	LinkedStringNode *last;
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

void chord_print_circle(Server *srv);

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
void activate_finger(Server *srv, Finger *f);
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
#define pack_chord_header(buf, type, msg) \
	pack_header(buf, CHORD_WIRE_VERSION, type, (const ProtobufCMessage *)msg)

int pack_header(uchar *buf, int version, int type, const ProtobufCMessage *msg);
int pack_addr_discover(uchar *buf, uchar *ticket, int ticket_len);
int pack_addr_discover_reply(uchar *buf, uchar *ticket, int ticket_len,
							 in6_addr *addr);
int pack_data(uchar *buf, int last, uchar ttl, chordID *id, ushort len,
			  const uchar *data);
int pack_fs(uchar *buf, uchar *ticket, int ticket_len, uchar ttl,
			in6_addr *addr, ushort port);
int pack_fs_reply(uchar *buf, uchar *ticket, int ticket_len, in6_addr *addr,
				  ushort port);
int pack_stab(uchar *buf, in6_addr *addr, ushort port);
int pack_stab_reply(uchar *buf, in6_addr *addr, ushort port);
int pack_notify(uchar *buf);
int pack_ping(uchar *buf, uchar *ticket, int ticket_len, ulong time);
int pack_pong(uchar *buf, uchar *ticket, int ticket_len, ulong time);
void protobuf_c_message_print(const ProtobufCMessage *message, FILE *out);

/* process.c */
struct ChordPacketArgs
{
	Server *srv;
} __attribute__((__packed__));
typedef struct ChordPacketArgs ChordPacketArgs;

Node *next_route_node(Server *srv, chordID *id, int last, int *next_is_last);
int process_addr_discover(Header *header, ChordPacketArgs *args,
						  AddrDiscover *msg, Node *from);
int process_addr_discover_reply(Header *header, ChordPacketArgs *args,
								AddrDiscoverReply *msg, Node *from);
int process_data(Header *header, ChordPacketArgs *args, Data *msg, Node *from);
int process_fs(Header *header, ChordPacketArgs *args, FindSuccessor *msg,
			   Node *from);
int process_fs_reply(Header *header, ChordPacketArgs *args,
					 FindSuccessorReply *msg, Node *from);
int process_stab(Header *header, ChordPacketArgs *args, Stabilize *msg,
				 Node *from);
int process_stab_reply(Header *header, ChordPacketArgs *args,
					   StabilizeReply *msg, Node *from);
int process_notify(Header *header, ChordPacketArgs *args, Notify *msg,
				   Node *from);
int process_ping(Header *header, ChordPacketArgs *args, Ping *msg, Node *from);
int process_pong(Header *header, ChordPacketArgs *args, Pong *msg, Node *from);
void process_error(Header *header, ChordPacketArgs *args, void *msg, Node *from,
				   int error);

/* sendpkt.c */
void send_packet(Server *srv, in6_addr *addr, in_port_t port, int n,
				 uchar *buf);
void send_raw_v4(int sock, in6_addr *addr, in_port_t port, int n, uchar *buf);
void send_raw_v6(int sock, in6_addr *addr, in_port_t port, int n, uchar *buf);
void send_data(Server *srv, int last, uchar ttl, Node *np, chordID *id,
			   ushort n, const uchar *data);
void send_fs(Server *srv, uchar ttl, in6_addr *to_addr, ushort to_port,
			 in6_addr *addr, ushort port);
void send_fs_forward(Server *srv, uchar *ticket, int ticket_len, uchar ttl,
					 in6_addr *to_addr, ushort to_port, in6_addr *addr,
					 ushort port);
void send_fs_reply(Server *srv, uchar *ticket, int ticket_len,
				   in6_addr *to_addr, ushort to_port, in6_addr *addr,
				   ushort port);
void send_stab(Server *srv, in6_addr *to_addr, ushort to_port, in6_addr *addr,
			   ushort port);
void send_stab_reply(Server *srv, in6_addr *to_addr, ushort to_port,
					in6_addr *addr, ushort port);
void send_notify(Server *srv, in6_addr *to_addr, ushort to_port);
void send_ping(Server *srv, in6_addr *to_addr, ushort to_port, ulong time);
void send_pong(Server *srv, uchar *ticket, int ticket_len, in6_addr *to_addr,
			   ushort to_port, ulong time);
void send_addr_discover(Server *srv, in6_addr *to_addr, ushort to_port);
void send_addr_discover_reply(Server *srv, uchar *ticket, int ticket_len,
							  in6_addr *to_addr, ushort to_port);

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
chordID atoid(const char *str);
unsigned hash(chordID *id, unsigned n);
const char *chordID_to_str(chordID *id);
void print_chordID(FILE *out, chordID *id);
void print_two_chordIDs(FILE *out, char *preffix, chordID *id1,
						char *middle, chordID *id2,
						char *suffix);
void print_three_chordIDs(FILE *out, char *preffix, chordID *id1,
						  char *middle1, chordID *id2,
						  char *middle2, chordID *id3,
						  char *suffix);
void print_node(FILE *out, Node *node, char *prefix, char *suffix);
void print_finger(FILE *out, Finger *f, char *prefix, char *suffix);
void print_finger_list(FILE *out, Finger *fhead, char *prefix, char *suffix);
void print_server(FILE *out, Server *s, char *prefix, char *suffix);
void print_send(FILE *out, Server *srv, char *send_type, chordID *id, in6_addr *addr,
				ushort port);
void print_process(FILE *out, Server *srv, char *process_type, chordID *id, in6_addr *addr,
				   ushort port);
void print_fun(FILE *out, Server *srv, char *fun_name, chordID *id);
void print_current_time(FILE *out, char *prefix, char *suffix);
int match_key(chordID *key_array, int num_keys, chordID *key);
int v6_addr_equals(const in6_addr *addr1, const in6_addr *addr2);
void v6_addr_copy(in6_addr *dest, const in6_addr *src);
void v6_addr_set(in6_addr *dest, const uchar *src);
char *buf_to_str(const uchar *buf, int len);
char *buf_to_hex(const uchar *buf, int len);

int pack_ticket(const uchar *salt, int salt_len, int hash_len, const uchar *out,
				const char *fmt, ...);
int verify_ticket(const uchar *salt, int salt_len, int hash_len,
				  const uchar *ticket_buf, int ticket_len, const char *fmt,
				  ...);

void get_data_id(chordID *id, const uchar *buf, int n);
void get_address_id(chordID *id, in6_addr *addr, ushort port);
int verify_address_id(chordID *id, in6_addr *addr, ushort port);

/* str.c */
LinkedString *lstr_empty();
LinkedString *lstr_new(const char *fmt, ...);
void lstr_free(LinkedString *str);
void lstr_add(LinkedString *str, const char *fmt, ...);
char *lstr_flat(LinkedString *str);

#define LogMessageTo(l_ctx, level, header, msg) \
{ \
	StartLogTo(l_ctx, level); \
	protobuf_c_message_print(msg, l_ctx->fp); \
	EndLogTo(l_ctx); \
}

#define LogMessage(level, header, msg)   LogMessageTo(get_logger_for_file(__FILE__), level, header, msg)
#define LogMessageAs(level, header, msg) LogMessageTo(get_logger(name), level, header, msg)

#if LOG_LEVEL <= LOG_LEVEL_FATAL && !defined DISABLED_ALL_LOGS
	#define LogString(level, lstr) \
		{ \
			char *LogString__str = lstr_flat(lstr); \
			Log(level, LogString__str); \
			free(LogString__str); \
		}
#else
	#define LogString(level, lstr)
#endif

#ifdef __cplusplus
}
#endif

#endif /* INCL_CHORD_H */
