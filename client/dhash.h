#ifndef DHASH_H
#define DHASH_H

#include <openssl/x509.h>
#include <netinet/in.h>

#undef DHASH_MESSAGE_DEBUG
#undef DHASH_CONTROL_MESSAGE_DEBUG

typedef void (*dhash_request_reply_handler)(void *ctx, int code,
											const uchar *name, int name_len);

struct Dispatcher;
struct event;
struct event_base;
struct Server;
struct Transfer;
typedef struct DHash DHash;
typedef struct DHashPacketArgs DHashPacketArgs;

struct DHash
{
	struct Server **servers;
	int *chord_tunnel_socks;
	int nservers;

	Transfer *trans_head;

	char *files_path;
	int control_sock;

	struct event_base *ev_base;
	struct event *control_sock_event;

	struct Dispatcher *control_dispatcher;
	struct Dispatcher *chord_dispatcher;

	STACK_OF(X509) *cert_stack;
};

struct DHashPacketArgs
{
	ChordPacketArgs chord_args;
	DHash *dhash;
} __attribute__((__packed__));

enum
{
	DHASH_CLIENT_REQUEST = 0,
	DHASH_CLIENT_REQUEST_REPLY,
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
	DHASH_PUSH,
	DHASH_PUSH_REPLY,
};

// in send.cpp
extern void dhash_client_send_request(int sock, const uchar *name,
									  int name_len);
// in pack.cpp
extern int dhash_client_unpack_request_reply(uchar *buf, int n, void *ctx,
											 dhash_request_reply_handler handler);

DHash *new_dhash(const char *files_path, const char *cert_path);
int dhash_start(DHash *dhash, char **conf_files, int nservers);

int dhash_stat_local_file(DHash *dhash, const uchar *file, int file_len,
						  struct stat *stat_buf);
int dhash_local_file_exists(DHash *dhash, const uchar *file, int file_len);
int dhash_local_file_size(DHash *dhash, const uchar *file, int file_len);

#endif
