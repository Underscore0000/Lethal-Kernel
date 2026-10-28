/*
 * Lethal Kernel - Network Syscalls
 *
 * v5.0: System call interface for networking.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "socket.h"
#include "socket_ops.h"
#include "../../process/syscall.h"
#include "../../core/console.h"
#include <linux/socket.h>

/* ============================================================================
 * Syscall Implementations (REAL)
 * ============================================================================ */

int sys_socket(int domain, int type, int protocol)
{
    return socket_create(domain, type, protocol);
}

int sys_bind(int sockfd, const struct sockaddr *addr, uint32_t addrlen)
{
    return socket_bind(sockfd, addr, addrlen);
}

int sys_connect(int sockfd, const struct sockaddr *addr, uint32_t addrlen)
{
    return socket_connect(sockfd, addr, addrlen);
}

int sys_listen(int sockfd, int backlog)
{
    return socket_listen(sockfd, backlog);
}

int sys_accept(int sockfd, struct sockaddr *addr, uint32_t *addrlen)
{
    return socket_accept(sockfd, addr, addrlen);
}

int sys_send(int sockfd, const void *buf, size_t len, int flags)
{
    return socket_send(sockfd, buf, len, flags);
}

int sys_recv(int sockfd, void *buf, size_t len, int flags)
{
    return socket_recv(sockfd, buf, len, flags);
}

int sys_sendto(int sockfd, const void *buf, size_t len, int flags,
                const struct sockaddr *dest_addr, uint32_t addrlen)
{
    return socket_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
}

int sys_recvfrom(int sockfd, void *buf, size_t len, int flags,
                  struct sockaddr *src_addr, uint32_t *addrlen)
{
    return socket_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
}

int sys_close_socket(int sockfd)
{
    return socket_close(sockfd);
}

int sys_setsockopt(int sockfd, int level, int optname, const void *optval, uint32_t optlen)
{
    return socket_setsockopt(sockfd, level, optname, optval, optlen);
}

int sys_getsockopt(int sockfd, int level, int optname, void *optval, uint32_t *optlen)
{
    return socket_getsockopt(sockfd, level, optname, optval, optlen);
}