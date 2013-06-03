#ifndef CHORD_OPTS_H
#define CHORD_OPTS_H

#ifdef __cplusplus
extern "C" {
#endif

#undef CHORD_MESSAGE_DEBUG
#undef CHORD_PRINT_LONG_IDS
#undef CHORD_PRINT_LONG_TIME

enum {
	CHORD_WIRE_VERSION = 1,
	TICKET_TIMEOUT = 1000000,		   /* seconds for which a ticket is valid */
	TICKET_HASH_LEN = 4,
	TICKET_SALT_LEN = 16,
	NFINGERS     = CHORD_ID_BITS,  /* # fingers per node */
	NSUCCESSORS  = 8,              /* # successors kept */
	NPREDECESSORS = 3,             /* # predecessors kept */
	ADDR_DISCOVER_INTERVAL = 1*1000000, /* in micros */
	STABILIZE_PERIOD = 3*1000000,  /* in micros */
	BUFSIZE      = 65535,          /* buffer for packets */
	MAX_WELLKNOWN = 50,            /* maximum number of other known servers
									*  (read from configuration file)
									*/
	MAX_PASSIVE_FINGERS = 20,	   /* maximum number of fingers to keep that
									* have yet to respond to ping
									*/
	PING_THRESH = 5,               /* this many unanswered pings are allowed */
	DEF_TTL      = 32,             /* default TTL for multi-hop packets */
};

#ifdef __cplusplus
}
#endif

#endif
