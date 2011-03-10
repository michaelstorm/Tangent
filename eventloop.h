#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Timer Timer;
typedef struct Socket Socket;
typedef struct Event Event;

typedef int (*timer_func)(void *ctx);
typedef int (*socket_func)(void *ctx, int sock);
typedef int (*event_func)(void *sub, void *pub, void *ctx, int event);

struct Timer
{
	uint64_t time;
	void *ctx;
	timer_func func;
	Timer *next;
};

struct Socket
{
	int sock;
	void *ctx;
	socket_func func;
	int type;
	Socket *next;
};

struct Event
{
	int event;
	void *sub;
	void *pub;
	event_func func;
	Event *next;
};

enum
{
	SOCKET_READ = 0,
	SOCKET_WRITE,
	SOCKET_EXCEPT,
};

void init_global_eventqueue();
void eventqueue_push_timer(uint64_t time, void *ctx, timer_func func);
void eventqueue_listen_socket(int sock, void *ctx, socket_func func, int type);
void eventqueue_subscribe(void *sub, void *pub, int e, event_func func);
void eventqueue_publish(void *pub, void *ctx, int e);
void eventqueue_wait(uint64_t wait_time);
void eventqueue_loop();

Timer *new_timer(uint64_t time, void *ctx, timer_func func);
Socket *new_socket(int sock, void *ctx, socket_func func, int type);
Event *new_event(void *sub, void *pub, int e, event_func func);

#ifdef __cplusplus
}
#endif

#endif
