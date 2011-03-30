#include <assert.h>
#include <event2/event.h>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <udt>
#include "chord.h"
#include "dhash.h"
#include "transfer.h"
using namespace std;

/* Trigger events to call these so that the callbacks run in the main thread. */
static void call_success_cb(evutil_socket_t sock, short what, void *arg)
{
	Transfer *trans = (Transfer *)arg;
	trans->success_cb(trans, trans->cb_arg);
}

static void call_fail_cb(evutil_socket_t sock, short what, void *arg)
{
	Transfer *trans = (Transfer *)arg;
	trans->fail_cb(trans, trans->cb_arg);
}

Transfer *new_transfer(int local_port, const in6_addr *addr, ushort port,
					   const char *dir)
{
	Transfer *trans = (Transfer *)calloc(1, sizeof(Transfer));
	trans->type = TRANSFER_IDLE;
	trans->file = NULL;

	trans->dir = (char *)malloc(strlen(dir)+1);
	strcpy(trans->dir, dir);

	v6_addr_copy(&trans->remote_addr, addr);
	trans->remote_port = port;

	trans->udt_sock = UDT::socket(V4_MAPPED(addr) ? AF_INET : AF_INET6,
								  SOCK_STREAM, 0);
	if (trans->udt_sock == UDT::INVALID_SOCK) {
		fprintf(stderr, "UDT::bind error: %s\n",
				UDT::getlasterror().getErrorMessage());
		return NULL;
	}

	int yes = true;
	UDT::setsockopt(trans->udt_sock, 0, UDT_RENDEZVOUS, &yes, sizeof(yes));

	sockaddr_in local_addr;
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(local_port);
	local_addr.sin_addr.s_addr = INADDR_ANY;
	memset(&local_addr.sin_zero, '\0', 8);

	if (UDT::ERROR == UDT::bind(trans->udt_sock, (struct sockaddr *)&local_addr,
								sizeof(local_addr))) {
		fprintf(stderr, "UDT::bind error: %s\n",
				UDT::getlasterror().getErrorMessage());
		return NULL;
	}

	return trans;
}

void free_transfer(Transfer *trans)
{
	if (trans) {
		if (UDT::ERROR == UDT::close(trans->udt_sock))
			cerr << "UDT::close: " << UDT::getlasterror().getErrorMessage();

		if (trans->success_cb)
			event_free(trans->success_ev);
		if (trans->fail_cb)
			event_free(trans->fail_ev);

		free(trans->file);
		free(trans);
	}
}

void transfer_set_callbacks(Transfer *trans, transfer_event_fn success_cb,
							transfer_event_fn fail_cb, void *cb_arg,
							struct event_base *ev_base)
{
	if (trans->success_ev)
		event_free(trans->success_ev);

	if (trans->fail_ev)
		event_free(trans->fail_ev);

	if (success_cb)
		trans->success_ev = event_new(ev_base, -1, 0, call_success_cb, trans);
	else
		trans->success_ev = NULL;

	if (fail_cb)
		trans->fail_ev = event_new(ev_base, -1, 0, call_fail_cb, trans);
	else
		trans->fail_ev = NULL;

	trans->success_cb = success_cb;
	trans->fail_cb = fail_cb;
	trans->cb_arg = cb_arg;
}

void transfer_start_receiving(Transfer *trans, const char *file)
{
	trans->file = (char *)malloc(strlen(file)+1);
	strcpy(trans->file, file);

	trans->type = TRANSFER_RECEIVE;
	pthread_create(&trans->thread, NULL, transfer_connect, trans);
}

void transfer_start_sending(Transfer *trans)
{
	trans->type = TRANSFER_SEND;
	pthread_create(&trans->thread, NULL, transfer_connect, trans);
}

static int blocking_recv_buf(Transfer *trans, uchar *buf, int size)
{
	int n, received = 0;
	while (received < size) {
		n = UDT::recv(trans->udt_sock, (char *)buf, size-received, 0);
		if (n == UDT::ERROR) {
			cerr << "UDT::recv: " << UDT::getlasterror().getErrorMessage();
			return 0;
		}

		received += n;
	}
	return 1;
}

static int blocking_send_buf(Transfer *trans, const uchar *buf, int size)
{
	int n, sent = 0;
	while (sent < size) {
		n = UDT::send(trans->udt_sock, (const char *)buf, size, 0);
		if (n == UDT::ERROR) {
			cerr << "UDT::send: " << UDT::getlasterror().getErrorMessage();
			return 0;
		}

		sent += n;
	}
	return 1;
}

static int transfer_receive(Transfer *trans)
{
	// send length of file name
	short name_len = (short)strlen(trans->file);
	short net_name_len = htons(name_len);
	if (!blocking_send_buf(trans, (uchar *)&net_name_len, 2))
		return 0;

	// send file name
	if (!blocking_send_buf(trans, (uchar *)trans->file, name_len))
		return 0;

	// receive file size
	int size;
	if (!blocking_recv_buf(trans, (uchar *)&size, 4))
		return 0;
	size = ntohl(size);

	char path[1024];
	strcpy(path, trans->dir);
	strcat(path, "/");
	strcat(path, trans->file);

	// open file
	fstream ofs(path, ios_base::out);

	// receive file
	int64_t zero = 0;
	if (UDT::ERROR == UDT::recvfile(trans->udt_sock, ofs, zero, size)) {
		cerr << "UDT::recvfile: " << UDT::getlasterror().getErrorMessage();
		return 0;
	}

	fprintf(stderr, "received file\n");

	return 1;
}

static int transfer_send(Transfer *trans)
{
	// receive length of file name
	short name_len;
	if (!blocking_recv_buf(trans, (uchar *)&name_len, 2))
		return 0;	// receive file name

	name_len = ntohs(name_len);

	// receive file name
	char buf[name_len+1];
	if (!blocking_recv_buf(trans, (uchar *)buf, name_len))
		return 0;
	buf[name_len] = '\0';

	trans->file = (char *)malloc(strlen(trans->dir)+name_len+2);
	strcpy(trans->file, buf);

	char path[1024];
	strcpy(path, trans->dir);
	strcat(path, "/");
	strcat(path, trans->file);

	// open file, get file size
	fstream ifs(path, ios_base::in);
	ifs.seekg(0, ios::end);
	int size = ifs.tellg();
	ifs.seekg(0, ios::beg);

	// send file size
	int net_size = htonl(size);
	if (!blocking_send_buf(trans, (uchar *)&net_size, 4))
		return 0;

	// send file
	int64_t zero = 0;
	if (UDT::ERROR == UDT::sendfile(trans->udt_sock, ifs, zero, size)) {
		cerr << "UDT::sendfile: " << UDT::getlasterror().getErrorMessage();
		return 0;
	}

	fprintf(stderr, "sent file\n");

	return 1;
}

void *transfer_connect(void *arg)
{
	Transfer *trans = (Transfer *)arg;

	int err;
	if (V4_MAPPED(&trans->remote_addr)) {
		sockaddr_in serv_addr;
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(trans->remote_port);
		serv_addr.sin_addr.s_addr = to_v4addr(&trans->remote_addr);
		memset(&serv_addr.sin_zero, '\0', 8);

		err = UDT::connect(trans->udt_sock, (sockaddr*)&serv_addr,
						   sizeof(serv_addr));
	}
	else {
		sockaddr_in6 serv_addr;
		serv_addr.sin6_family = AF_INET6;
		serv_addr.sin6_port = htons(trans->remote_port);
		v6_addr_copy(&serv_addr.sin6_addr, &trans->remote_addr);

		err = UDT::connect(trans->udt_sock, (sockaddr*)&serv_addr,
						   sizeof(serv_addr));
	}

	if (err == UDT::ERROR) {
		fprintf(stderr, "UDT::connect: %s\n",
				UDT::getlasterror().getErrorMessage());
		return NULL;
	}

	int succeeded;
	switch (trans->type) {
	case TRANSFER_RECEIVE:
		succeeded = transfer_receive(trans);
		break;
	case TRANSFER_SEND:
		succeeded = transfer_send(trans);
		break;
	default:
		fprintf(stderr, "invalid transfer type %02x\n", trans->type);
		succeeded = 0;
	}

	if (succeeded) {
		if (trans->success_cb)
			event_active(trans->success_ev, 0, 1);
		else
			free_transfer(trans);
	}
	else {
		if (trans->fail_cb)
			event_active(trans->fail_ev, 0, 1);
		else
			free_transfer(trans);
	}

	return NULL;
}
