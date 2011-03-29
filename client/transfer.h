#ifndef TRANSFER_H
#define TRANSFER_H

#include <pthread.h>

struct DHash;
struct Transfer;
typedef struct Transfer Transfer;

enum
{
	TRANSFER_IDLE = 0,
	TRANSFER_SENDING,
	TRANSFER_RECEIVING,
	TRANSFER_COMPLETE,
	TRANSFER_FAILED,
};

enum
{
	TRANSFER_SEND = 0x1,
	TRANSFER_RECEIVE = 0x2,
	TRANSFER_PUSH = 0x4,
};

struct Transfer
{
	struct DHash *dhash;
	char *file;

	int chord_sock;
	int udt_sock;

	in6_addr remote_addr;
	unsigned short remote_port;
	unsigned short chord_port;

	int type;

	pthread_t thread;
};

Transfer *new_transfer(DHash *dhash, int chord_sock, const in6_addr *addr,
					   ushort port, int type);
void free_transfer(Transfer *trans);

void transfer_start_receiving(Transfer *trans, const char *file);
void transfer_start_sending(Transfer *trans);
void *transfer_connect(void *arg);

#endif
