#ifndef HOSTS_H
#define HOSTS_H

#include "chord/visibility.h"

#ifdef __cplusplus
extern "C" {
#endif

in_addr_t get_addr();
int resolve_v6name(const char *name, in6_addr *v6addr) DLL_PUBLIC;
void set_socket_nonblocking(int sock);
void bind_v6socket(int sock, const in6_addr *addr, ushort port);
void bind_v4socket(int sock, ulong addr, ushort port);

#ifdef __cplusplus
}
#endif

#endif
