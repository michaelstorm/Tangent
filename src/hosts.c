/* host registration (mainly for testing) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <netdb.h>
extern int h_errno;
#include <net/if.h>
#include "chord/chord.h"
#include "chord/util.h"

#define TRIVIAL_LOCAL_ADDR	"127.0.0.1"
#define MAX_NUM_INTERFACES	3
#define IFNAME_LEN		256

/***************************************************************************
 *
 * Purpose: Get IP address of local machine by ioctl on eth0-ethk
 *
 * Return: As an unsigned long in network byte order
 *
 **************************************************************************/
static uint32_t get_local_addr_eth()
{
	int i, tempfd;
	struct sockaddr_in addr;
	char ifname[IFNAME_LEN];
	struct ifreq ifr;

	for (i = 0; i < MAX_NUM_INTERFACES; i++) {
		sprintf(ifname, "eth%d", i);
		strcpy(ifr.ifr_name, ifname);
		tempfd = socket(AF_INET, SOCK_DGRAM, 0);

		if (ioctl(tempfd, SIOCGIFFLAGS, (char *)&ifr) != -1) {
			if ((ifr.ifr_flags & IFF_UP) != 0) {
				if (ioctl(tempfd, SIOCGIFADDR, (char *)&ifr) != -1) {
					addr = *((struct sockaddr_in *) &ifr.ifr_addr);
					close(tempfd);
					return addr.sin_addr.s_addr;
				}
			}
		}
		close(tempfd);
	}

	return inet_addr(TRIVIAL_LOCAL_ADDR);
}

/***************************************************************************
 *
 * Purpose: Get the IP address of an arbitrary machine
 *	given the name of the machine
 *
 * Return: As an unsigned long in network byte order
 *
 **************************************************************************/
static uint32_t name_to_addr(const char *name)
{
	int i;
	struct hostent *hptr = gethostbyname(name);
	if (!hptr) {
		weprintf("gethostbyname(%s) failed", name);
	}
	else {
		for (i = 0; i < hptr->h_length/sizeof(uint32_t); i++) {
			uint32_t addr = *((uint32_t *) hptr->h_addr_list[i]);
			if (inet_addr(TRIVIAL_LOCAL_ADDR) != addr)
				return addr;
		}
	}
	return 0;
}

/***************************************************************************
 *
 * Purpose: Get IP address of local machine by uname/gethostbyname
 *
 * Return: As an unsigned long in network byte order
 *
 **************************************************************************/
static uint32_t get_local_addr_uname()
{
	struct utsname myname;
	uint32_t addr;

	if (uname(&myname) < 0)
		weprintf("uname failed:");
	else
		addr = name_to_addr(myname.nodename);

	if (addr == 0)
		return inet_addr(TRIVIAL_LOCAL_ADDR);
	else
		return addr;
}

/***************************************************************************
 *
 * Purpose: Get IP address of local machine
 *
 * Return: As an unsigned long in network byte order
 *
 **************************************************************************/
static uint32_t get_local_addr()
{
	uint32_t addr;

	/* First try uname/gethostbyname */
	if ((addr = get_local_addr_uname()) != inet_addr(TRIVIAL_LOCAL_ADDR))
		return addr;
	/* If that is unsuccessful, try ioctl on eth interfaces */
	if ((addr = get_local_addr_eth()) != inet_addr(TRIVIAL_LOCAL_ADDR))
		return addr;

	/* This is hopeless, return TRIVIAL_IP */
	return inet_addr(TRIVIAL_LOCAL_ADDR);
}

/***********************************************************************/

/* get_addr: get IP address of server */
in_addr_t get_addr()
{
	return (in_addr_t)get_local_addr();
}

void set_socket_nonblocking(int sock)
{
	int flags = fcntl(sock, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(sock, F_SETFL, flags);
}

int resolve_v6name(const char *name, in6_addr *v6addr)
{
	in_addr v4addr;

	// if the name is just an address, convert it to binary
	if (inet_pton(AF_INET6, name, v6addr))
		return 0;
	else if (inet_pton(AF_INET, name, &v4addr)) {
		to_v6addr(v4addr.s_addr, v6addr);
		return 0;
	}

	// otherwise, resolve the name
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6; // accept v6 addrs

	struct addrinfo *result;
	if (getaddrinfo(name, NULL, NULL, &result) != 0) {
		weprintf("getaddrinfo(%s) failed:", name);
		return -1;
	}

	if (result->ai_family == AF_INET)
		to_v6addr(((struct sockaddr_in *)result->ai_addr)->sin_addr.s_addr,
				  v6addr);
	else
		v6_addr_copy(v6addr,
					 &((struct sockaddr_in6 *)result->ai_addr)->sin6_addr);

	freeaddrinfo(result);
	return 0;
}

void bind_v6socket(int sock, const in6_addr *addr, ushort port)
{
	int reuse = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	struct sockaddr_in6 sin;
	memset(&sin, 0, sizeof(sin));

	sin.sin6_family = AF_INET;
	sin.sin6_port = htons(port);
	v6_addr_copy(&sin.sin6_addr, addr);

	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		eprintf("bind failed:");
}

void bind_v4socket(int sock, ulong addr, ushort port)
{
	int reuse = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = htonl(addr);

	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		eprintf("bind failed:");
}
