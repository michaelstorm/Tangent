#ifndef DHASH_H
#define DHASH_H

typedef void (*dhash_request_reply_handler)(void *ctx, char code,
											const char *file);

struct Server;

typedef struct
{
	struct Server **servers;
	int *chord_tunnel_socks;
	int nservers;

	char *files_path;
	int control_sock;
} DHash;

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
