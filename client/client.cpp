#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <event2/event.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include "chord/chord.h"
#include "chord/grid.h"
#include "chord/logger/color.h"
#include "dhash.h"

static int control_sock;

logger_ctx_t *l;

void process_reply(void *ctx, int code, const uchar *name, int name_len)
{
	switch (code) {
	case DHASH_CLIENT_REPLY_LOCAL:
		printf("%s is local\n", name);
		break;
	case DHASH_CLIENT_REPLY_SUCCESS:
		printf("%s retrieved successfully\n", name);
		break;
	case DHASH_CLIENT_REPLY_FAILURE:
		printf("%s retrieval failed\n", name);
		break;
	default:
		printf("unknown code\n");
		break;
	}
}

void handle_reply(evutil_socket_t sock, short what, void *arg)
{
	uchar buf[1024];
	int n;

	if ((n = read(sock, buf, 1024)) < 0)
		perror("reading file request reply");

	dhash_client_unpack_request_reply(buf, n, arg, process_reply);
}

void handle_request(evutil_socket_t sock, short what, void *arg)
{
	int n;
	uchar file[128];

	if ((n = read(sock, file, sizeof(file)-1)) < 0)
		perror("reading file request reply");

	if (n > 0)
		dhash_client_send_request(control_sock, file, n-1); // skip newline
}

static void event_logging_cb(int severity, const char *msg)
{
	int level;
	switch (severity)
	{
		case EVENT_LOG_DEBUG: level = -1; break;
		case EVENT_LOG_MSG:   level = DEBUG;  break;
		case EVENT_LOG_WARN:  level = WARN;  break;
		case EVENT_LOG_ERR:   level = ERROR; break;
		default:			  level = WARN;  break;
	}
	
	LogAs("libevent", level, msg);
}

void print_separator()
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	
	char line[w.ws_col+1];
	int i;
	for (i = 0; i < w.ws_col; i++)
		line[i] = '=';
	line[w.ws_col] = '\0';
	
	cfprintf(stdout, FG_BLACK|BG_WHITE|MOD_INTENSE_BG, "%s\n", line);
}

int main(int argc, char **argv)
{
	logger_init();
	print_separator();
	chord_check_library_versions();
	
	l = get_logger("dhash");
	
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

	event_set_log_callback(&event_logging_cb);
	event_enable_debug_logging(EVENT_DBG_ALL);
	
	LogInfoTo(l, "Started DHash client");

	// fork the dhash/chord process
	DHash *dhash = new_dhash(argv[1], argv[2]);
	control_sock = dhash_start(dhash, argv+3, argc-3);	

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
	fprintf(stderr, "starting event loop...\n");
	event_base_dispatch(ev_base);
	return 0;
}
