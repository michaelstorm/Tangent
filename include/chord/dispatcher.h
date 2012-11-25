#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <stddef.h>
#include <stdint.h>
#include "chord/chord_api.h"
#include "messages.pb-c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Dispatcher Dispatcher;

struct Node;
struct Server;
struct _ProtobufCAllocator;
typedef void *(*unpack_fn)(struct _ProtobufCAllocator *, size_t,
						   const uint8_t *);
typedef int (*process_fn)(Header *, void **, void *, struct Node *);
typedef void (*unpack_error_fn)(void *, int, unsigned char *, int,
								struct Node *from);
typedef void (*process_error_fn)(Header *, void **, void *, struct Node *from,
								 int error);

struct packet_handler;
struct odd_packet_handler;

struct Dispatcher
{
	struct packet_handler *handlers;
	struct odd_packet_handler *odd;
	int size;

	unpack_error_fn unpack_error;
	process_error_fn process_error;
};

Dispatcher *new_dispatcher(int size);
void free_dispatcher(Dispatcher *d);

const char *dispatcher_get_packet_name(Dispatcher *d, int value);

void dispatcher_set_error_handlers(Dispatcher *d, unpack_error_fn u_err,
								   process_error_fn p_err);
void dispatcher_set_packet_body(Dispatcher *d, int value, char *name, void *arg,
								unpack_fn unpack, process_fn process);
int dispatcher_set_packet_handlers(Dispatcher *d, int value, unpack_fn unpack,
								   process_fn process);
void dispatcher_create_handler(Dispatcher *d, int value, char *name, void *arg,
							   unpack_fn unpack, process_fn process);
int dispatcher_push_arg(Dispatcher *d, int value, void *arg);
void *dispatcher_pop_arg(Dispatcher *d, int value);
int dispatcher_get_type(uchar *buf, int n);
int dispatch_packet(Dispatcher *d, uchar *buf, int n, struct Node *from,
					int *process_ret);

#define dispatcher_set_packet(d, value, arg, unpack, process) \
	dispatcher_set_packet_body(d, value, (char *)#value, arg, (unpack_fn)unpack, \
							   (process_fn)process)

#ifdef __cplusplus
}
#endif

#endif