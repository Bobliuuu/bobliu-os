#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H 1
#include <sys/types.h>
#include <sys/uio.h>
typedef unsigned int socklen_t;
typedef unsigned short sa_family_t;
#define AF_UNSPEC 0
#define AF_UNIX 1
#define AF_LOCAL 1
#define AF_INET 2
#define AF_INET6 10
#define PF_UNSPEC AF_UNSPEC
#define PF_UNIX AF_UNIX
#define PF_INET AF_INET
#define PF_INET6 AF_INET6
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_NONBLOCK 0x800
#define SOCK_CLOEXEC 0x80000
#define SOL_SOCKET 1
#define SO_REUSEADDR 2
#define SO_ERROR 4
#define SO_KEEPALIVE 9
#define SO_BROADCAST 6
#define SO_SNDBUF 7
#define SO_RCVBUF 8
#define SO_REUSEPORT 15
#define SOMAXCONN 128
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2
#define MSG_PEEK 2
#define MSG_NOSIGNAL 0x4000
#define MSG_DONTWAIT 0x40
struct sockaddr { sa_family_t sa_family; char sa_data[14]; };
struct sockaddr_storage { sa_family_t ss_family; char __pad[126]; };
struct msghdr { void* msg_name; socklen_t msg_namelen; struct iovec* msg_iov; int msg_iovlen; void* msg_control; socklen_t msg_controllen; int msg_flags; };
struct cmsghdr { socklen_t cmsg_len; int cmsg_level; int cmsg_type; };
#ifdef __cplusplus
extern "C" {
#endif
int socket(int domain, int type, int protocol);
int socketpair(int domain, int type, int protocol, int sv[2]);
int bind(int fd, const struct sockaddr* a, socklen_t l);
int listen(int fd, int backlog);
int accept(int fd, struct sockaddr* a, socklen_t* l);
int connect(int fd, const struct sockaddr* a, socklen_t l);
int getsockname(int fd, struct sockaddr* a, socklen_t* l);
int getpeername(int fd, struct sockaddr* a, socklen_t* l);
int setsockopt(int fd, int lvl, int opt, const void* v, socklen_t l);
int getsockopt(int fd, int lvl, int opt, void* v, socklen_t* l);
int shutdown(int fd, int how);
ssize_t send(int fd, const void* b, size_t n, int fl);
ssize_t recv(int fd, void* b, size_t n, int fl);
ssize_t sendmsg(int fd, const struct msghdr* m, int fl);
ssize_t recvmsg(int fd, struct msghdr* m, int fl);
ssize_t sendto(int fd, const void* b, size_t n, int fl, const struct sockaddr* a, socklen_t l);
ssize_t recvfrom(int fd, void* b, size_t n, int fl, struct sockaddr* a, socklen_t* l);
#ifdef __cplusplus
}
#endif
#endif
