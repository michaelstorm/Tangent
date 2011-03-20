#include <stdio.h>
#include <event2/event.h>
#include <sys/wait.h>
#include "chord_api.h"
#include "dhash.h"
#include "eventloop.h"

void process_reply(void *ctx, char code, const char *file)
{
	switch (code) {
	case DHASH_CLIENT_REPLY_LOCAL:
		printf("%s is local\n", file);
		break;
	default:
		printf("unknown code\n");
		break;
	}
}

void handle_reply(evutil_socket_t sock, short what, void *arg)
{
	dhash_client_process_request_reply(sock, arg, process_reply);
}

int main(int argc, char **argv)
{
	DHash *dhash = new_dhash("files");
	int sock = dhash_start(dhash, argv+1, 1 /*argc-1 */);

	struct event_base *ev_base = event_base_new();
	struct event *reply_event = event_new(ev_base, sock, EV_READ|EV_PERSIST,
										  handle_reply, NULL);
	event_add(reply_event, NULL);

	if (argc > 2) {
		struct timeval timeout;
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;

		event_base_loopexit(ev_base, &timeout);
		event_base_dispatch(ev_base);

		dhash_client_request_file(sock, "west");
	}

	event_base_dispatch(ev_base);

	return 0;
}
