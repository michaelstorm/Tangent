#ifndef DHASH_TRANSFER_H
#define DHASH_TRANSFER_H

struct DHash;
struct Transfer;
typedef struct Transfer Transfer;

struct Transfer
{
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
};

Transfer *new_transfer(char *file, int chord_sock, const in6_addr *addr,
					   ushort port);
void free_transfer(Transfer *trans);
void transfer_stop(Transfer *trans, int state);
int transfer_receive(Transfer *trans, int sock);
int transfer_send(Transfer *trans, int sock);
void transfer_start_receiving(Transfer *trans, const char *dir, int size);
void transfer_start_sending(Transfer *trans, const char *dir);

#endif
