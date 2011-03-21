#ifndef TRANSFER_H
#define TRANSFER_H

struct DHash;
struct Transfer;
typedef struct Transfer Transfer;

typedef void (*transfer_statechange_cb)(Transfer *trans, int old_state,
										void *arg);

enum
{
	TRANSFER_IDLE = 0,
	TRANSFER_SENDING,
	TRANSFER_RECEIVING,
	TRANSFER_COMPLETE,
	TRANSFER_FAILED,
};

struct Transfer
{
	struct event_base *ev_base;
	struct event *chord_sock_event;

	char *file;
	FILE *fp;

	int chord_sock;
	int udt_sock;

	long received;
	long size;

	in6_addr remote_addr;
	unsigned short remote_port;
	unsigned short chord_port;

	struct DHash *dhash;
	Transfer *next;

	int state;
	transfer_statechange_cb statechange_cb;
	void *statechange_arg;
};

Transfer *new_transfer(struct event_base *ev_base, const char *file,
					   int chord_sock, const in6_addr *addr, ushort port);
void free_transfer(Transfer *trans);
void transfer_set_statechange_cb(Transfer *trans, transfer_statechange_cb cb,
								 void *arg);
void transfer_stop(Transfer *trans, int state);
int transfer_receive(Transfer *trans, int sock);
void transfer_send(evutil_socket_t sock, short what, void *arg);
void transfer_start_receiving(Transfer *trans, const char *dir, int size);
void transfer_start_sending(Transfer *trans, const char *dir);

#endif
