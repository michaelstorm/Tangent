#include <stdlib.h>
#include <string.h>
#include "chord.h"
#include "dispatcher.h"

struct packet_handler
{
	char *name;
	void **process_args;
	int nargs;

	unpack_fn unpack;
	process_fn process;
};

struct odd_packet_handler
{
	int value;
	struct odd_packet_handler *next;
	struct packet_handler handler;
};

Dispatcher *new_dispatcher(int size)
{
	Dispatcher *d = malloc(sizeof(Dispatcher));
	d->handlers = calloc(1, sizeof(struct packet_handler)*size);
	d->size = size;
	d->odd = 0;

	d->unpack_error = 0;
	d->process_error = 0;
	return d;
}

static void free_odd(struct odd_packet_handler *odd)
{
	if (odd) {
		free(odd->handler.name);
		free(odd->handler.process_args);
		free(odd);
	}
}

void free_dispatcher(Dispatcher *d)
{
	if (d) {
		int i;
		for (i = 0; i < d->size; i++) {
			free(d->handlers[i].name);
			free(d->handlers[i].process_args);
		}

		struct odd_packet_handler *odd = d->odd;
		while (odd) {
			struct odd_packet_handler *next = odd->next;
			free_odd(odd);
			odd = next;
		}
		free(d);
	}
}

static struct packet_handler *get_handler(Dispatcher *d, int value)
{
	if (value < d->size)
		return &d->handlers[value];

	struct odd_packet_handler *odd = d->odd;
	while (odd) {
		if (odd->value < value)
			odd = odd->next;
		else if (odd->value == value)
			return &odd->handler;
		else
			break;
	}
	return 0;
}

const char *dispatcher_get_packet_name(Dispatcher *d, int value)
{
	struct packet_handler *handler;
	if ((handler = get_handler(d, value)))
		return handler->name;
	else
		return NULL;
}

void dispatcher_set_error_handlers(Dispatcher *d, unpack_error_fn u_err,
								   process_error_fn p_err)
{
	d->unpack_error = u_err;
	d->process_error = p_err;
}

static void init_handler(struct packet_handler *handler, int value, char *name,
						 void *arg, unpack_fn unpack, process_fn process)
{
	handler->name = malloc(strlen(name)+1);
	strcpy(handler->name, name);

	handler->process_args = malloc(sizeof(void *));
	handler->process_args[0] = arg;
	handler->nargs = 1;

	handler->unpack = unpack;
	handler->process = process;
}

void dispatcher_set_packet_body(Dispatcher *d, int value, char *name, void *arg,
								unpack_fn unpack, process_fn process)
{
	struct packet_handler *handler;
	if (!(handler = get_handler(d, value)))
		dispatcher_create_handler(d, value, name, arg, unpack, process);
	else {
		free(handler->name);
		free(handler->process_args);
		init_handler(handler, value, name, arg, unpack, process);
	}
}

int dispatcher_set_packet_handlers(Dispatcher *d, int value, unpack_fn unpack,
								   process_fn process)
{
	struct packet_handler *handler;
	if (!(handler = get_handler(d, value)))
		return 0;

	handler->unpack = unpack;
	handler->process = process;
	return 1;
}

void dispatcher_create_handler(Dispatcher *d, int value, char *name, void *arg,
							   unpack_fn unpack, process_fn process)
{
	if (value < d->size)
		init_handler(&d->handlers[value], value, name, arg, unpack, process);
	else {
		struct odd_packet_handler *odd;
		if (!d->odd) {
			d->odd = malloc(sizeof(struct packet_handler));
			d->odd->next = 0;
			odd = d->odd;
		}
		else {
			while (odd->next && odd->next->value > value)
				odd = odd->next;

			if (!odd->next) {
				odd->next = malloc(sizeof(struct packet_handler));
				odd->next->next = 0;
				odd = odd->next;
			}
			else if (odd->next->value == value)
				odd = odd->next;
			else {
				struct odd_packet_handler *next = odd->next;
				odd->next = malloc(sizeof(struct packet_handler));
				odd->next->next = next;
				odd = odd->next;
			}
		}

		odd->value = value;
		init_handler(&odd->handler, value, name, arg, unpack, process);
	}
}

int dispatcher_push_arg(Dispatcher *d, int value, void *arg)
{
	struct packet_handler *handler;
	if (!(handler = get_handler(d, value)))
		return 0;

	handler->process_args = realloc(handler->process_args,
									sizeof(void *)*(++handler->nargs));
	handler->process_args[handler->nargs-1] = arg;
	return 1;
}

void *dispatcher_pop_arg(Dispatcher *d, int value)
{
	struct packet_handler *handler;
	if (!(handler = get_handler(d, value)))
		return 0;

	return handler->process_args[--handler->nargs];
}

int dispatcher_get_type(uchar *buf, int n)
{
	Header *header = header__unpack(NULL, n, buf);
	return header->type;
}

int dispatch_packet(Dispatcher *d, uchar *buf, int n, Node *from,
					int *process_ret)
{
	Header *header = header__unpack(NULL, n, buf);

	struct packet_handler *handler = get_handler(d, header->type);
	if (!handler)
		return 0;

	void *msg = handler->unpack(NULL, header->payload.len,
								header->payload.data);
	if (!msg) {
		if (d->unpack_error)
			d->unpack_error(handler->process_args, header->type,
							header->payload.data, header->payload.len, from);
		return 1;
	}

	int ret = handler->process(header, handler->process_args, msg, from);
	if (process_ret)
		*process_ret = ret;

	if (ret) {
		if (d->process_error)
			d->process_error(header, handler->process_args, msg, from, ret);
		return 1;
	}

	protobuf_c_message_free_unpacked((ProtobufCMessage *)msg, NULL);
	header__free_unpacked(header, NULL);

	return 1;
}
