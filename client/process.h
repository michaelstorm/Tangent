#ifndef DHASH_PROCESS_H
#define DHASH_PROCESS_H

int dhash_process_query_reply_success(DHash *dhash, Server *srv, uchar *data,
									  int n, Node *from);
int dhash_process_query(DHash *dhash, Server *srv, in6_addr *reply_addr,
						ushort reply_port, const char *file, Node *from);
void dhash_process_client_query(DHash *dhash, const char *file);

#endif
