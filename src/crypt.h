#ifndef UTIL_H
#define UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

int pack_ticket_impl(const uchar *salt, int salt_len, int hash_len, const uchar *out,
					 const char *args_str, const char *fmt, ...);
int verify_ticket_impl(const uchar *salt, int salt_len, int hash_len,
					   const uchar *ticket_buf, int ticket_len, const char *fmt, const char *args_str, 
					   ...);

#define pack_ticket(salt, salt_len, hash_len, out, fmt, ...) pack_ticket_impl(salt, salt_len, hash_len, out, #__VA_ARGS__, fmt, ##__VA_ARGS__)
#define verify_ticket(salt, salt_len, hash_len, ticket_buf, ticket_len, fmt, ...) verify_ticket_impl(salt, salt_len, hash_len, ticket_buf, ticket_len, #__VA_ARGS__, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif