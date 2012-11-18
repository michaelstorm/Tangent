#ifndef CHORD_H
#define CHORD_H

#include <event2/event.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <inttypes.h>
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
	TICKET_TIMEOUT = 1000000,		   /* seconds for which a ticket is valid */
	TICKET_HASH_LEN = 4,
	TICKET_SALT_LEN = 16,
	ADDRESS_SALTS = 3,			   /* number of IDs an address can have */
	NFINGERS     = CHORD_ID_BITS,  /* # fingers per node */
	NSUCCESSORS  = 8,              /* # successors kept */
	NPREDECESSORS = 3,             /* # predecessors kept */
	ADDR_DISCOVER_INTERVAL = 1*1000000, /* in micros */
	STABILIZE_PERIOD = 3*1000000,  /* in micros */
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

typedef void (*send_func_t)(int sock, in6_addr *addr, in_port_t port, int n, uchar *buf);

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
	send_func_t send_func;

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
int chord_check_library_versions();
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

/* join.c */
void discover_addr(evutil_socket_t sock, short what, void *arg);
void join(Server *srv, FILE *fp);

/* stabilize.c */
void stabilize(evutil_socket_t sock, short what, void *arg);

/* str.c */
LinkedString *lstr_empty();
LinkedString *lstr_new(const char *fmt, ...);
void lstr_free(LinkedString *str);
void lstr_add(LinkedString *str, const char *fmt, ...);
char *lstr_flat(LinkedString *str);

#define LogMessageTo(l_ctx, level, header, msg) \
{ \
	StartLogTo(l_ctx, level); \
	PartialLogTo(l_ctx, "%s\n", header); \
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
