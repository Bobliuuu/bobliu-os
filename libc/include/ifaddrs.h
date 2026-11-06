#ifndef _IFADDRS_H
#define _IFADDRS_H 1
#include <sys/socket.h>
struct ifaddrs { struct ifaddrs* ifa_next; char* ifa_name; unsigned ifa_flags; struct sockaddr* ifa_addr; struct sockaddr* ifa_netmask; struct sockaddr* ifa_dstaddr; void* ifa_data; };
int getifaddrs(struct ifaddrs** ifap);
void freeifaddrs(struct ifaddrs* ifa);
#endif
