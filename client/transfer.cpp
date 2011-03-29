#include <assert.h>
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

Transfer *new_transfer(DHash *dhash, int chord_sock, const in6_addr *addr,
					   ushort port, int type)
{
	Transfer *trans = (Transfer *)malloc(sizeof(Transfer));
	trans->dhash = dhash;
	trans->file = NULL;

	v6_addr_copy(&trans->remote_addr, addr);
	trans->remote_port = port;

	trans->type = type;

	trans->udt_sock = UDT::socket(V4_MAPPED(addr) ? AF_INET : AF_INET6,
								  SOCK_STREAM, 0);

	int yes = true;
	UDT::setsockopt(trans->udt_sock, 0, UDT_RENDEZVOUS, &yes, sizeof(yes));

	if (UDT::ERROR == UDT::bind(trans->udt_sock, chord_sock)) {
		fprintf(stderr, "bind error: %s\n",
				UDT::getlasterror().getErrorMessage());
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

void transfer_start_receiving(Transfer *trans, const char *file)
{
	trans->file = (char *)malloc(strlen(file)+1);
	strcpy(trans->file, file);

	pthread_create(&trans->thread, NULL, transfer_connect, trans);
}

void transfer_start_sending(Transfer *trans)
{
	pthread_create(&trans->thread, NULL, transfer_connect, trans);
}

static int blocking_recv_buf(Transfer *trans, uchar *buf, int size)
{
	int n, received = 0;
	while (received < size) {
		n = UDT::recv(trans->udt_sock, (char *)buf, size-received, 0);
		if (n == UDT::ERROR) {
			cerr << "recv: " << UDT::getlasterror().getErrorMessage();
			return 0;
		}

		received += n;
	}
	return 1;
}

static int send_buf(Transfer *trans, const uchar *buf, int size)
{
	if (UDT::ERROR == UDT::send(trans->udt_sock, (const char *)buf, size, 0)) {
		cerr << "send: " << UDT::getlasterror().getErrorMessage();
		return 0;
	}
	return 1;
}

static void transfer_receive(Transfer *trans)
{
	// send length of file name
	short name_len = (short)strlen(trans->file);
	short net_name_len = htons(name_len);
	send_buf(trans, (uchar *)&net_name_len, 2);

	// send file name
	send_buf(trans, (uchar *)trans->file, name_len);

	// receive file size
	int size;
	blocking_recv_buf(trans, (uchar *)&size, 4);
	size = ntohl(size);

	char path[1024];
	strcpy(path, trans->dhash->files_path);
	strcat(path, "/");
	strcat(path, trans->file);

	// open file
	fstream ofs(path, ios_base::out);

	// receive file
	int64_t zero = 0;
	if (UDT::ERROR == UDT::recvfile(trans->udt_sock, ofs, zero, size)) {
		cerr << "recvfile: " << UDT::getlasterror().getErrorMessage();
		return;
	}
	fprintf(stderr, "finished receiving!\n");
}

static void transfer_send(Transfer *trans)
{
	// receive length of file name
	short name_len;
	blocking_recv_buf(trans, (uchar *)&name_len, 2);
	name_len = ntohs(name_len);

	// receive file name
	char buf[name_len+1];
	blocking_recv_buf(trans, (uchar *)buf, name_len);
	buf[name_len] = '\0';

	trans->file = (char *)malloc(strlen(trans->dhash->files_path+name_len+2));
	strcpy(trans->file, buf);

	char path[1024];
	strcpy(path, trans->dhash->files_path);
	strcat(path, "/");
	strcat(path, trans->file);

	// open file, get file size
	fstream ifs(path, ios_base::in);
	ifs.seekg(0, ios::end);
	int size = ifs.tellg();
	ifs.seekg(0, ios::beg);

	// send file size
	int net_size = htonl(size);
	if (UDT::ERROR == UDT::send(trans->udt_sock, (char *)&net_size, 4, 0)) {
		cerr << "send: " << UDT::getlasterror().getErrorMessage();
		return;
	}

	// send file
	int64_t zero = 0;
	if (UDT::ERROR == UDT::sendfile(trans->udt_sock, ifs, zero, size)) {
		cerr << "sendfile: " << UDT::getlasterror().getErrorMessage();
		return;
	}
	fprintf(stderr, "finished sending!\n");
}

/* Connect within a thread until UDT implements non-blocking connection setup;
   otherwise, peers can hang us for the 30 seconds until the handshake times
   out.
 */
void *transfer_connect(void *arg)
{
	Transfer *trans = (Transfer *)arg;

	int err;
	if (V4_MAPPED(&trans->remote_addr)) {
		sockaddr_in serv_addr;
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(trans->remote_port);
		serv_addr.sin_addr.s_addr = to_v4addr(&trans->remote_addr);
		memset(&(serv_addr.sin_zero), '\0', 8);

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
		fprintf(stderr, "connect: %s\n", UDT::getlasterror().getErrorMessage());
		return NULL;
	}

	if (trans->type & TRANSFER_RECEIVE)
		transfer_receive(trans);
	else if (trans->type & TRANSFER_SEND)
		transfer_send(trans);
	else {
		fprintf(stderr, "invalid transfer type %02x\n", trans->type);
		abort();
	}
}
