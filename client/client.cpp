#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <event2/event.h>
#include <sys/wait.h>
#include "chord_api.h"
#include "chord.h"
#include "grid.h"
#include "dhash.h"

static int control_sock;

void process_reply(void *ctx, char code, const char *file)
{
	switch (code) {
	case DHASH_CLIENT_REPLY_LOCAL:
		printf("%s is local\n", file);
		break;
	case DHASH_CLIENT_REPLY_SUCCESS:
		printf("%s retrieved successfully\n", file);
		break;
	case DHASH_CLIENT_REPLY_FAILURE:
		printf("%s retrieval failed\n", file);
		break;
	default:
		printf("unknown code\n");
		break;
	}
}

void handle_reply(evutil_socket_t sock, short what, void *arg)
{
	dhash_client_unpack_request_reply(sock, arg, process_reply);
}

void handle_request(evutil_socket_t sock, short what, void *arg)
{
	int n;
	char file[128];

	if ((n = read(sock, file, sizeof(file)-1)) < 0)
		perror("reading file request reply");

	if (n > 0) {
		file[n-1] = '\0'; // overwrite newline
		dhash_client_send_request(control_sock, file);
	}
}

int main(int argc, char **argv)
{
	if (strcmp(argv[1], "--butterfly") == 0) {
		long double diam;
		if (argc > 2)
			sscanf(argv[2], "%Lf", &diam);
		else
			diam = 80;

#define ROW_RATIO ((long double).5)
		struct grid *g = new_grid(diam, diam*ROW_RATIO+1);
		struct circle *c = new_circle((diam-1)/2, (diam-1)/2, diam-1, PI,
									  ROW_RATIO, 10);
		draw_butterfly(g, c, '*', 0, 24*PI);
		print_grid(stdout, g);
		return 0;
	}

	// fork the dhash/chord process
	DHash *dhash = new_dhash("files");
	control_sock = dhash_start(dhash, argv+1, 1 /*argc-1 */);

	// create an event_base that works with events on file descriptors
	struct event_config *cfg = event_config_new();
	event_config_require_features(cfg, EV_FEATURE_FDS);
	struct event_base *ev_base = event_base_new_with_config(cfg);
	event_config_free(cfg);

	// listen on the control socket
	struct event *reply_event = event_new(ev_base, control_sock,
										  EV_READ|EV_PERSIST, handle_reply,
										  NULL);
	event_add(reply_event, NULL);

	// listen on stdin
	struct event *request_event = event_new(ev_base, 0, EV_READ|EV_PERSIST,
											handle_request, dhash);
	event_add(request_event, NULL);

	// start event loop
	event_base_dispatch(ev_base);
	return 0;
}
