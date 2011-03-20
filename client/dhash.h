#ifndef DHASH_H
#define DHASH_H

#include <netinet/in.h>

typedef void (*dhash_request_reply_handler)(void *ctx, char code,
											const char *file);

struct event;
struct event_base;
struct Server;
struct Transfer;
typedef struct DHash DHash;

struct DHash
{
	struct Server **servers;
	int *chord_tunnel_socks;
	int nservers;

	Transfer *trans_head;

	char *files_path;
	int control_sock;

	struct event_base *ev_base;
	struct event *udt_sock_event;
	struct event *control_sock_event;
};

enum
{
	DHASH_CLIENT_QUERY = 0,
};

enum
{
	DHASH_CLIENT_REPLY_LOCAL = 0,
	DHASH_CLIENT_REPLY_SUCCESS,
	DHASH_CLIENT_REPLY_FAILURE,
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

void dhash_add_transfer(DHash *dhash, Transfer *trans);
void dhash_remove_transfer(DHash *dhash, Transfer *remove);

int dhash_stat_local_file(DHash *dhash, const char *file,
						  struct stat *stat_buf);
int dhash_local_file_exists(DHash *dhash, const char *file);
int dhash_local_file_size(DHash *dhash, const char *file);

int dhash_process_query_reply_success(DHash *dhash, struct Server *srv,
									  unsigned char *data, int n,
									  struct Node *from);
int dhash_process_query(DHash *dhash, struct Server *srv, unsigned char *data,
						int n, struct Node *from);
void dhash_process_client_query(DHash *dhash, const char *file);

#endif
