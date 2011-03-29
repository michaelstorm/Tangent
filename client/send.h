#ifndef DHASH_SEND_H
#define DHASH_SEND_H

void dhash_send_control_packet(struct DHash *dhash, int code, const char *file);
void dhash_send_push(DHash *dhash, const char *name, long file_size);
void dhash_send_query(struct DHash *dhash, const char *file);
void dhash_send_query_reply_success(struct DHash *dhash, Server *srv,
									in6_addr *addr, ushort port,
									const char *file);
void dhash_send_query_reply_failure(struct DHash *dhash, Server *srv,
									in6_addr *addr, ushort port,
									const char *file);
void dhash_send_push_reply(DHash *dhash, Server *srv, in6_addr *addr,
						   ushort port, const char *file);

void dhash_send_control_query_success(struct DHash *dhash, const char *file);
void dhash_send_control_query_failure(struct DHash *dhash, const char *file);
void dhash_client_send_request(int sock, const char *file);

#endif
