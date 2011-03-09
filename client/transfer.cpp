#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <udt>
#include "chord.h"
#include "dhash.h"
#include "transfer.h"

Transfer *new_transfer(char *file, int chord_sock, const in6_addr *addr,
					   ushort port)
{
	Transfer *trans = (Transfer *)malloc(sizeof(Transfer));
	trans->chord_sock = chord_sock;
	trans->next = NULL;
	trans->file = (char *)malloc(strlen(file)+1);
	strcpy(trans->file, file);

	trans->received = 0;
	trans->size = 0;

	trans->state = DHASH_TRANSFER_IDLE;

	trans->udt_sock = UDT::socket(V4_MAPPED(addr) ? AF_INET : AF_INET6,
								  SOCK_STREAM, 0);

	int yes = true;
	UDT::setsockopt(trans->udt_sock, 0, UDT_RENDEZVOUS, &yes, sizeof(yes));
	UDT::setsockopt(trans->udt_sock, 0, UDT_SNDSYN, &yes, sizeof(yes));
	UDT::setsockopt(trans->udt_sock, 0, UDT_RCVSYN, &yes, sizeof(yes));

	if (UDT::ERROR == UDT::bind(trans->udt_sock, chord_sock)) {
		fprintf(stderr, "bind error: %s\n",
				UDT::getlasterror().getErrorMessage());
		return NULL;
	}

	int err;
	if (V4_MAPPED(addr)) {
		sockaddr_in serv_addr;
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(port);
		serv_addr.sin_addr.s_addr = to_v4addr(addr);
		memset(&(serv_addr.sin_zero), '\0', 8);

		err = UDT::connect(trans->udt_sock, (sockaddr*)&serv_addr,
						   sizeof(serv_addr));
	}
	else {
		sockaddr_in6 serv_addr;
		serv_addr.sin6_family = AF_INET6;
		serv_addr.sin6_port = htons(port);
		v6_addr_copy(&serv_addr.sin6_addr, addr);

		err = UDT::connect(trans->udt_sock, (sockaddr*)&serv_addr,
						   sizeof(serv_addr));
	}

	if (err == UDT::ERROR) {
		fprintf(stderr, "connect: %s\n", UDT::getlasterror().getErrorMessage());
		return NULL;
	}

	return trans;
}

void free_transfer(Transfer *trans)
{
	if (trans) {
		free(trans->file);
		free(trans);
	}
}

void transfer_stop(Transfer *trans, int state)
{
	if (trans->state == DHASH_TRANSFER_RECEIVING
		|| trans->state == DHASH_TRANSFER_SENDING) {
		fclose(trans->fp);
		UDT::close(trans->udt_sock);
	}

	trans->state = state;
}

int transfer_receive(Transfer *trans, int sock)
{
	assert(trans->state == DHASH_TRANSFER_RECEIVING);

	uchar buf[1024];
	int len = UDT::recv(trans->udt_sock, (char *)buf, sizeof(buf), 0);

	printf("received %ld/%ld of \"%s\"\n", trans->received, trans->size,
		   trans->file);

	if (fwrite(buf, 1, len, trans->fp) < len) {
		weprintf("writing to \"%\":", trans->file);
		transfer_stop(trans, DHASH_TRANSFER_FAILED);
		return 1;
	}

	trans->received += len;
	if (trans->received == trans->size) {
		printf("done receiving \"%s\"\n", trans->file);

		transfer_stop(trans, DHASH_TRANSFER_COMPLETE);
		eventqueue_publish(trans, 0, DHASH_TRANSFER_EVENT_COMPLETE);
		return 1;
	}
	else if (trans->received >= trans->size) {
		printf("received size %ld greater than expected size %ld for \"%s\"\n",
			   trans->received, trans->size, trans->file);

		transfer_stop(trans, DHASH_TRANSFER_FAILED);
		eventqueue_publish(trans, 0, DHASH_TRANSFER_EVENT_FAILED);
		return 1;
	}

	return 0;
}

int transfer_send(Transfer *trans, int sock)
{
	assert(trans->state == DHASH_TRANSFER_SENDING);

	uchar buf[1024];
	int n = fread(buf, 1, sizeof(buf), trans->fp);
	printf("read %d bytes from %s\n", n, trans->file);

	if (n < 0) {
		weprintf("reading from \"%s\":", trans->file);

		transfer_stop(trans, DHASH_TRANSFER_FAILED);
		eventqueue_publish(trans, 0, DHASH_TRANSFER_EVENT_FAILED);
		return 1;
	}
	else if (n == 0) {
		printf("done reading from \"%s\"\n", trans->file);

		transfer_stop(trans, DHASH_TRANSFER_COMPLETE);
		eventqueue_publish(trans, 0, DHASH_TRANSFER_EVENT_COMPLETE);
		return 1;
	}

	if (UDT::ERROR == UDT::send(trans->udt_sock, (char *)buf, n, 0)) {
		fprintf(stderr, "send: %s\n", UDT::getlasterror().getErrorMessage());
		return 1;
	}

	return 0;
}

void transfer_start_receiving(Transfer *trans, const char *dir, int size)
{
	assert(trans->state == DHASH_TRANSFER_IDLE);

	char path[1024];
	strcpy(path, dir);
	strcat(path, "/");
	strcat(path, trans->file);

	trans->size = size;

	if (NULL == (trans->fp = fopen(path, "wb"))) {
		weprintf("could not open \"%s\" for reading:", path);
		return;
	}

	trans->state = DHASH_TRANSFER_RECEIVING;
	printf("started receiving %s\n", path);
}

void transfer_start_sending(Transfer *trans, const char *dir)
{
	assert(trans->state == DHASH_TRANSFER_IDLE);

	char path[1024];
	strcpy(path, dir);
	strcat(path, "/");
	strcat(path, trans->file);

	if (NULL == (trans->fp = fopen(path, "rb"))) {
		weprintf("could not open \"%s\" for reading:", path);
		return;
	}

	trans->state = DHASH_TRANSFER_SENDING;
	printf("started sending %s\n", path);
	eventqueue_listen_socket(trans->chord_sock, trans,
							 (socket_func)transfer_send, SOCKET_WRITE);
}
