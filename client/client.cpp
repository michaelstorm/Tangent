#include <stdio.h>
#include <sys/wait.h>
#include "chord_api.h"
#include "dhash.h"
#include "eventloop.h"

void process_reply(void *ctx, char code, const char *file)
{
	switch (code) {
	case DHASH_REPLY_LOCAL:
		printf("%s is local\n", file);
		break;
	default:
		printf("unknown code\n");
		break;
	}
}

int handle_reply(void *ctx, int sock)
{
	dhash_client_process_request_reply(sock, ctx, process_reply);
}

int main(int argc, char **argv)
{
	DHash *dhash = new_dhash("files");
	int sock = dhash_start(dhash, argv+1, 1 /*argc-1 */);

	init_global_eventqueue();
	eventqueue_listen_socket(sock, 0, handle_reply, SOCKET_READ);

	if (argc > 2) {
		eventqueue_wait(5*1000000);
		dhash_client_request_file(sock, "west");
	}

	eventqueue_loop();

	return 0;
}
