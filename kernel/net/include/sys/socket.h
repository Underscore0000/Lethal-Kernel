/*
 * Lethal Kernel - Socket API (Userspace)
 *
 * v5.0: BSD socket interface for userspace programs.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_LINUX_SOCKET_H
#define LETHAL_LINUX_SOCKET_H

#include <stdint.h>

/* ============================================================================
 * Socket Address Families
 * ============================================================================ */

#define AF_UNSPEC       0
#define AF_UNIX         1
#define AF_INET         2
#define AF_INET6        10

/* ============================================================================
 * Socket Types
 * ============================================================================ */

#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3
#define SOCK_RDM        4
#define SOCK_SEQPACKET  5

/* ============================================================================
 * Socket Protocol Numbers
 * ============================================================================ */

#define IPPROTO_IP      0
#define IPPROTO_ICMP    1
#define IPPROTO_IGMP    2
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17
#define IPPROTO_RAW     255

/* ============================================================================
 * Socket Options
 * ============================================================================ */

#define SOL_SOCKET      1
#define SO_REUSEADDR    2
#define SO_KEEPALIVE    9
#define SO_RCVTIMEO     20
#define SO_SNDTIMEO     21

/* ============================================================================
 * Socket Address Structures
 * ============================================================================ */

struct sockaddr {
    uint16_t sa_family;
    char sa_data[14];
};

struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    uint8_t sin_zero[8];
};

struct sockaddr_in6 {
    uint16_t sin6_family;
    uint16_t sin6_port;
    uint32_t sin6_flowinfo;
    uint8_t sin6_addr[16];
    uint32_t sin6_scope_id;
};

/* ============================================================================
 * Syscall Numbers
 * ============================================================================ */

#define SYS_SOCKET      110
#define SYS_BIND        111
#define SYS_CONNECT     112
#define SYS_LISTEN      113
#define SYS_ACCEPT      114
#define SYS_SEND        115
#define SYS_RECV        116
#define SYS_SENDTO      117
#define SYS_RECVFROM    118
#define SYS_CLOSE       119
#define SYS_GETSOCKNAME 120
#define SYS_GETPEERNAME 121
#define SYS_SETSOCKOPT  122
#define SYS_GETSOCKOPT  123

/* ============================================================================
 * Userspace Socket Functions
 * ============================================================================ */

int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *addr, uint32_t addrlen);
int connect(int sockfd, const struct sockaddr *addr, uint32_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, uint32_t *addrlen);
int send(int sockfd, const void *buf, size_t len, int flags);
int recv(int sockfd, void *buf, size_t len, int flags);
int sendto(int sockfd, const void *buf, size_t len, int flags,
           const struct sockaddr *dest_addr, uint32_t addrlen);
int recvfrom(int sockfd, void *buf, size_t len, int flags,
             struct sockaddr *src_addr, uint32_t *addrlen);
int closesocket(int sockfd);

/* Helper functions */
uint32_t htonl(uint32_t hostlong);
uint16_t htons(uint16_t hostshort);
uint32_t ntohl(uint32_t netlong);
uint16_t ntohs(uint16_t netshort);

#endif /* LETHAL_LINUX_SOCKET_H */