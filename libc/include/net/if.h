#ifndef _NET_IF_H
#define _NET_IF_H 1
#define IF_NAMESIZE 16
#define IFF_UP 1
#define IFF_LOOPBACK 8
#define IFF_RUNNING 0x40
unsigned int if_nametoindex(const char* name);
char* if_indextoname(unsigned int idx, char* name);
#endif
