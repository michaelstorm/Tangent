#ifndef TRANSFER_H
#define TRANSFER_H

#include <pthread.h>

struct event;
struct DHash;
struct Transfer;
typedef struct Transfer Transfer;

typedef void (*transfer_event_fn)(Transfer *trans, void *arg);

enum
{
	TRANSFER_SEND = 0x1,
	TRANSFER_RECEIVE = 0x2,
	TRANSFER_PUSH = 0x4,
};

struct Transfer
{
	char *dir;
	char *file;

	transfer_event_fn success_cb;
	transfer_event_fn fail_cb;
	void *cb_arg;
	struct event *success_ev;
	struct event *fail_ev;

	int chord_sock;
	int udt_sock;

	in6_addr remote_addr;
	unsigned short remote_port;
	unsigned short chord_port;

	int type;

	pthread_t thread;
};

Transfer *new_transfer(int chord_sock, const in6_addr *addr, ushort port,
					   int type, const char *dir,
					   transfer_event_fn success_cb,
					   transfer_event_fn fail_cb, void *func_arg,
					   struct event_base *ev_base);
void free_transfer(Transfer *trans);

void transfer_start_receiving(Transfer *trans, const char *file);
void transfer_start_sending(Transfer *trans);
void *transfer_connect(void *arg);

#endif
