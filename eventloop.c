#include "eventloop.h"
#include "gen_utils.h"
#include <assert.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/select.h>

#include <stdio.h>

typedef struct
{
	Timer *timer_head;
	Socket *socket_head;
	Event *event_head;

	fd_set read_socket_set;
	fd_set write_socket_set;
	fd_set except_socket_set;

	int nfds;
} EventQueue;

static EventQueue *global_queue = NULL;

EventQueue *new_eventqueue()
{
	EventQueue *queue = malloc(sizeof(EventQueue));
	queue->timer_head = NULL;
	queue->socket_head = NULL;
	queue->event_head = NULL;

	FD_ZERO(&queue->read_socket_set);
	FD_ZERO(&queue->write_socket_set);
	FD_ZERO(&queue->except_socket_set);

	queue->nfds = 0;
	return queue;
}

void free_eventqueue(EventQueue *queue)
{
	Timer *timer = queue->timer_head;
	while (timer) {
		Timer *prev = timer;
		timer = timer->next;
		free(prev);
	}
	free(queue);
}

void init_global_eventqueue()
{
	assert(global_queue == NULL);
	global_queue = new_eventqueue();
}

void eventqueue_push_timer(uint64_t time, void *ctx, timer_func func)
{
	time += wall_time();

	Timer *push_timer = new_timer(time, ctx, func);
	if (!global_queue->timer_head)
		global_queue->timer_head = push_timer;
	else if (global_queue->timer_head->time > time) {
		push_timer->next = global_queue->timer_head;
		global_queue->timer_head = push_timer;
	}
	else {
		Timer *timer = global_queue->timer_head;
		while (timer->next != NULL && timer->next->time >= time)
			timer = timer->next;
		push_timer->next = timer->next;
		timer->next = push_timer;
	}
}

Timer *eventqueue_pop_timer()
{
	Timer *timer = global_queue->timer_head;
	global_queue->timer_head = global_queue->timer_head->next;
	return timer;
}

void eventqueue_listen_socket(int sock, void *ctx, socket_func func, int type)
{
	Socket *s = new_socket(sock, ctx, func, type);

	if (!global_queue->socket_head)
		global_queue->socket_head = s;
	else {
		s->next = global_queue->socket_head;
		global_queue->socket_head = s;
	}

	switch (type) {
	case SOCKET_READ:
		FD_SET(sock, &global_queue->read_socket_set);
		break;
	case SOCKET_WRITE:
		FD_SET(sock, &global_queue->write_socket_set);
		break;
	case SOCKET_EXCEPT:
		FD_SET(sock, &global_queue->except_socket_set);
		break;
	}

	global_queue->nfds = MAX(global_queue->nfds-1, sock) + 1;
}

void eventqueue_subscribe(void *sub, void *pub, int e, event_func func)
{
	Event *event = new_event(sub, pub, e, func);
	if (global_queue->event_head == NULL)
		global_queue->event_head = event;
	else {
		event->next = global_queue->event_head;
		global_queue->event_head = event;
	}
}

void eventqueue_publish(void *pub, void *ctx, int e)
{
	Event *event = global_queue->event_head;
	while (event) {
		if (event->pub == pub && event->event == e)
			event->func(event->sub, pub, ctx, e);
		event = event->next;
	}
}

void eventqueue_wait(uint64_t wait_time)
{
	eventqueue_push_timer(wait_time, 0, 0);
	eventqueue_loop();
}

void eventqueue_loop()
{
	for (;;) {
		int nfound;
		fd_set readable = global_queue->read_socket_set;
		fd_set writable = global_queue->write_socket_set;
		fd_set except = global_queue->except_socket_set;

		if (!global_queue->timer_head)
			nfound = select(global_queue->nfds, &readable, &writable, &except,
							NULL);
		else {
			uint64_t stabilize_wait;
			uint64_t wtime = wall_time();
			if (global_queue->timer_head->time < wtime)
				stabilize_wait = 0;
			else
				stabilize_wait = global_queue->timer_head->time - wtime;

			struct timeval timeout;
			timeout.tv_sec = stabilize_wait / 1000000UL;
			timeout.tv_usec = stabilize_wait % 1000000UL;

			nfound = select(global_queue->nfds, &readable, &writable, &except,
							&timeout);
		}

		/* error */
		if (nfound < 0 && errno == EINTR)
			continue;
		/* one or more packets arrived */
		else if (nfound > 0) {
			Socket *s = global_queue->socket_head;
			while (s != NULL) {
				fd_set *interesting;
				switch (s->type) {
				case SOCKET_READ:
					interesting = &readable;
					break;
				case SOCKET_WRITE:
					interesting = &writable;
					break;
				case SOCKET_EXCEPT:
					interesting = &except;
					break;
				}

				if (FD_ISSET(s->sock, interesting)) {
					if (1 == s->func(s->ctx, s->sock)) {
						switch(s->type) {
						case SOCKET_READ:
							FD_CLR(s->sock, &global_queue->read_socket_set);
							break;
						case SOCKET_WRITE:
							FD_CLR(s->sock, &global_queue->write_socket_set);
							break;
						case SOCKET_EXCEPT:
							FD_CLR(s->sock, &global_queue->except_socket_set);
							break;
						}

						Socket *next = s->next;
						free(s);
						s = next;
						continue;
					}
				}

				s = s->next;
			}
		}

		uint64_t wtime = wall_time();
		int quit = 0;
		while (global_queue->timer_head
			   && global_queue->timer_head->time <= wtime) {
			Timer *timer = eventqueue_pop_timer();
			if (timer->func)
				timer->func(timer->ctx);
			else
				quit = 1;

			Timer *prev = timer;
			timer = timer->next;
			free(prev);
		}

		if (quit)
			return;
	}
}

Timer *new_timer(uint64_t time, void *ctx, timer_func func)
{
	Timer *timer = malloc(sizeof(Timer));
	timer->time = time;
	timer->ctx = ctx;
	timer->func = func;
	timer->next = NULL;
	return timer;
}

Socket *new_socket(int sock, void *ctx, socket_func func, int type)
{
	Socket *s = malloc(sizeof(Socket));
	s->sock = sock;
	s->ctx = ctx;
	s->func = func;
	s->type = type;
	s->next = NULL;
	return s;
}

Event *new_event(void *sub, void *pub, int e, event_func func)
{
	Event *event = malloc(sizeof(Event));
	event->sub = sub;
	event->pub = pub;
	event->event = e;
	event->func = func;
	return event;
}
