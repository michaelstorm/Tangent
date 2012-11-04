#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <udt/udt.h>
#include <unistd.h>
#include <pthread.h>
#include <event2/event.h>
#include <event2/thread.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include "chord.h"
#include "dhash.h"
#include "dispatcher.h"
#include "d_messages.pb-c.h"
#include "pack.h"
#include "process.h"
#include "send.h"
#include "transfer.h"

DHash *new_dhash(const char *files_path, const char *cert_path)
{
	DHash *dhash = (DHash *)malloc(sizeof(DHash));
	dhash->servers = NULL;
	dhash->nservers = 0;
	dhash->trans_head = NULL;

	dhash->control_dispatcher = new_dispatcher(DHASH_CLIENT_REQUEST+1);
	dispatcher_set_packet(dhash->control_dispatcher, DHASH_CLIENT_REQUEST,
						  dhash, client_request__unpack,
						  dhash_process_client_request);

	dhash->chord_dispatcher = new_dispatcher(DHASH_PUSH_REPLY+1);
	dispatcher_set_packet(dhash->chord_dispatcher, DHASH_QUERY, dhash,
						  query__unpack, dhash_process_query);
	dispatcher_set_packet(dhash->chord_dispatcher, DHASH_QUERY_REPLY_SUCCESS,
						  dhash, query_reply_success__unpack,
						  dhash_process_query_reply_success);
	dispatcher_set_packet(dhash->chord_dispatcher, DHASH_QUERY_REPLY_FAILURE,
						  dhash, query_reply_failure__unpack,
						  dhash_process_query_reply_failure);
	dispatcher_set_packet(dhash->chord_dispatcher, DHASH_PUSH, dhash,
						  push__unpack, dhash_process_push);
	dispatcher_set_packet(dhash->chord_dispatcher, DHASH_PUSH_REPLY, dhash,
						  push_reply__unpack, dhash_process_push_reply);

#ifdef DHASH_MESSAGE_DEBUG
	dispatcher_set_debug(dhash->chord_dispatcher, 1);
#endif

#ifdef DHASH_CONTROL_MESSAGE_DEBUG
	dispatcher_set_debug(dhash->control_dispatcher, 1);
#endif

	dhash->files_path = (char *)malloc(strlen(files_path)+1);
	strcpy(dhash->files_path, files_path);

	FILE *cert_fp = fopen(cert_path, "rb");
	if (!cert_fp) {
		eprintf("error opening signing certificate \"%s\":", cert_path);
		return NULL;
	}

	ERR_load_crypto_strings();

	X509 *cert = PEM_read_X509(cert_fp, NULL, NULL, NULL);
	if (!cert) {
		fprintf(stderr, "error parsing signing certificate \"%s\": %s\n",
				cert_path, ERR_error_string(ERR_get_error(), NULL));
		return NULL;
	}
	fclose(cert_fp);

	dhash->cert_stack = sk_X509_new_null();
	sk_X509_push(dhash->cert_stack, cert);

	return dhash;
}

int dhash_stat_local_file(DHash *dhash, const uchar *file, int file_len,
						  struct stat *stat_buf)
{
	char abs_file_path[strlen(dhash->files_path) + file_len + 1];

	strcpy(abs_file_path, dhash->files_path);
	strcpy(abs_file_path + strlen(dhash->files_path), "/");
	memcpy(abs_file_path + strlen(dhash->files_path)+1, file, file_len);
	abs_file_path[sizeof(abs_file_path)] = '\0';

	return stat(abs_file_path, stat_buf);
}

int dhash_local_file_exists(DHash *dhash, const uchar *file, int file_len)
{
	struct stat stat_buf;
	return dhash_stat_local_file(dhash, file, file_len, &stat_buf) == 0;
}

int dhash_local_file_size(DHash *dhash, const uchar *file, int file_len)
{
	struct stat stat_buf;
	assert(dhash_stat_local_file(dhash, file, file_len, &stat_buf) == 0);
	return stat_buf.st_size;
}

int dhash_start(DHash *dhash, char **conf_files, int nservers)
{
	int dhash_tunnel[2];
	if (socketpair(AF_UNIX, SOCK_DGRAM, 0, dhash_tunnel) < 0)
		eprintf("socket_pair failed:");

	dhash->control_sock = dhash_tunnel[1];

//	int f = fork();
//	if (f) {
//		fprintf(stderr, "child PID: %d\nparent PID: %d\n", f, getpid());
//		return dhash_tunnel[0];
//	}

	setprogname("dhash");
	srandom(getpid() ^ time(0));

	UDT::startup();

	evthread_use_pthreads();
	dhash->ev_base = event_base_new();

	dhash->servers = (Server **)malloc(sizeof(Server *)*nservers);
	dhash->chord_tunnel_socks = (int *)malloc(sizeof(int)*nservers);
	dhash->nservers = nservers;

	int i;
	for (i = 0; i < nservers; i++) {
		/*int chord_tunnel[2];
		if (socketpair(AF_UNIX, SOCK_DGRAM, 0, chord_tunnel) < 0)
			eprintf("socket_pair failed:");*/

		dhash->servers[i] = new_server(dhash->ev_base, 0 /*chord_tunnel[1]*/);
		dhash->chord_tunnel_socks[i] = 0 /*chord_tunnel[0]*/;

		Server *srv = dhash->servers[i];
		server_initialize_from_file(srv, conf_files[i]);
		server_initialize_socket(srv);

		dispatcher_set_packet_handlers(srv->dispatcher, CHORD_DATA,
									   (unpack_fn)data__unpack,
									   (process_fn)dhash_unpack_chord_data);
		dispatcher_push_arg(srv->dispatcher, CHORD_DATA, dhash);

		server_start(srv);
	}

	dhash->control_sock_event = event_new(dhash->ev_base, dhash->control_sock,
										  EV_READ|EV_PERSIST,
										  dhash_unpack_control_packet, dhash);
	event_add(dhash->control_sock_event, NULL);

	event_base_dispatch(dhash->ev_base);
}
