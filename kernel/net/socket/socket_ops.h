/*
 * Lethal Kernel - Socket Operations Header
 *
 * v5.0: Socket operation functions.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_SOCKET_SOCKET_OPS_H
#define LETHAL_NET_SOCKET_SOCKET_OPS_H

#include <stdint.h>
#include "socket.h"
#include <net/socket.h>

/* Socket option operations */
int socket_setsockopt(int fd, int level, int optname, const void *optval, uint32_t optlen);
int socket_getsockopt(int fd, int level, int optname, void *optval, uint32_t *optlen);

/* Socket info operations */
int socket_getsockname(int fd, struct sockaddr *addr, uint32_t *addrlen);
int socket_getpeername(int fd, struct sockaddr *addr, uint32_t *addrlen);

/* Socket IOCTL */
int socket_ioctl(int fd, uint32_t cmd, void *arg);

#endif /* LETHAL_NET_SOCKET_SOCKET_OPS_H */