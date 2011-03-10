#ifndef DHASH_SEND_H
#define DHASH_SEND_H

void dhash_send_control_packet(struct DHash *dhash, int code, const char *file);
void dhash_send_file_query(struct DHash *dhash, const char *file);
void dhash_send_query_reply_success(struct DHash *dhash, Server *srv,
									in6_addr *addr, ushort port,
									const char *file);
void dhash_send_query_reply_failure(struct DHash *dhash, Server *srv,
									in6_addr *addr, ushort port,
									const char *file);
int dhash_send_control_transfer_complete(struct DHash *dhash, Transfer *trans);
int dhash_send_control_transfer_failed(struct DHash *dhash, Transfer *trans);

#endif
