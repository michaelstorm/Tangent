#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <event2/event.h>
#include <event2/thread.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <time.h>
#include "chord/chord.h"
#include "chord/grid.h"
#include "chord/logger/color.h"
#include "sglib.h"

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

void create_chord_servers(struct event_base *ev_base, char *conf_file)
{
	struct ChordServerElement *srv_list = server_initialize_list_from_file(ev_base, conf_file);
	SGLIB_LIST_MAP_ON_ELEMENTS(struct ChordServerElement, srv_list, srv_elem, next, {
		server_initialize_socket(srv_elem->value);
		server_start(srv_elem->value);
	});
}

void init_logging()
{
	clog_init();
	print_separator();
}

void init_global_libevent()
{
	event_set_log_callback(&event_logging_cb);
	event_enable_debug_logging(EVENT_DBG_ALL);
}

int main(int argc, char **argv)
{
	srandom(getpid() ^ time(0));

	init_logging();
	clog_set_event_context("init");

	chord_check_library_versions();
	init_global_libevent();

	struct event_base *ev_base = event_base_new();
	create_chord_servers(ev_base, argv[1]);

	Debug("Starting event loop...");
	return event_base_dispatch(ev_base);
}
