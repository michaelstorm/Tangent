#ifndef DHASH_H
#define DHASH_H

#include <netinet/in.h>

typedef struct Transfer Transfer;
typedef void (*dhash_request_reply_handler)(void *ctx, char code,
											const char *file);

struct Server;
typedef struct DHash DHash;

struct Transfer
{
	char *file;
	FILE *fp;

	int down;
	int chord_sock;
	int udt_sock;

	in6_addr remote_addr;
	unsigned short remote_port;
	unsigned short chord_port;

	DHash *dhash;
	Transfer *next;
};

struct DHash
{
	struct Server **servers;
	int *chord_tunnel_socks;
	int nservers;

	Transfer *trans_head;

	char *files_path;
	int control_sock;
};

enum
{
	DHASH_REPLY_LOCAL = 0,
};

enum
{
	DHASH_QUERY = 0,
	DHASH_QUERY_REPLY_SUCCESS,
	DHASH_QUERY_REPLY_FAILURE,
};

DHash *new_dhash(const char *files_path);
int dhash_start(DHash *dhash, char **conf_files, int nservers);

void dhash_client_request_file(int sock, const char *file);
void dhash_client_process_request_reply(int sock, void *ctx,
										dhash_request_reply_handler handler);

#endif
