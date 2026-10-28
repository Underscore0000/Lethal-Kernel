/*
 * Lethal Kernel - Socket Operations Implementation
 *
 * v5.0: REAL socket option and info operations.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "socket_ops.h"
#include "socket.h"
#include <net/socket.h>
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>

/* ============================================================================
 * Socket Options (REAL)
 * ============================================================================ */

int socket_setsockopt(int fd, int level, int optname, const void *optval, uint32_t optlen)
{
    struct socket *sock = socket_find(fd);
    if (!sock) {
        serial_print("[socket_ops] Socket not found\n");
        return -1;
    }
    
    if (level == SOL_SOCKET) {
        switch(optname) {
            case SO_REUSEADDR:
                if (optlen >= sizeof(int)) {
                    sock->options |= SO_REUSEADDR;
                    serial_print("[socket_ops] SO_REUSEADDR enabled\n");
                }
                break;
            case SO_KEEPALIVE:
                if (optlen >= sizeof(int)) {
                    sock->options |= SO_KEEPALIVE;
                    serial_print("[socket_ops] SO_KEEPALIVE enabled\n");
                }
                break;
            case SO_RCVTIMEO:
                if (optlen >= sizeof(uint32_t)) {
                    sock->recv_timeout = *(uint32_t *)optval;
                    serial_print("[socket_ops] SO_RCVTIMEO set to ");
                    serial_print_dec(sock->recv_timeout);
                    serial_print("ms\n");
                }
                break;
            case SO_SNDTIMEO:
                if (optlen >= sizeof(uint32_t)) {
                    sock->send_timeout = *(uint32_t *)optval;
                    serial_print("[socket_ops] SO_SNDTIMEO set to ");
                    serial_print_dec(sock->send_timeout);
                    serial_print("ms\n");
                }
                break;
            default:
                serial_print("[socket_ops] Unknown socket option\n");
                return -1;
        }
        return 0;
    }
    
    /* Protocol-level options */
    if (level == IPPROTO_TCP && sock->type == SOCK_STREAM) {
        /* TCP specific options */
        serial_print("[socket_ops] TCP socket option not implemented\n");
        return -1;
    }
    
    if (level == IPPROTO_UDP && sock->type == SOCK_DGRAM) {
        /* UDP specific options */
        serial_print("[socket_ops] UDP socket option not implemented\n");
        return -1;
    }
    
    return -1;
}

int socket_getsockopt(int fd, int level, int optname, void *optval, uint32_t *optlen)
{
    struct socket *sock = socket_find(fd);
    if (!sock) {
        serial_print("[socket_ops] Socket not found\n");
        return -1;
    }
    
    if (!optval || !optlen) {
        return -1;
    }
    
    if (level == SOL_SOCKET) {
        switch(optname) {
            case SO_TYPE:
                if (*optlen >= sizeof(int)) {
                    *(int *)optval = sock->type;
                    *optlen = sizeof(int);
                    return 0;
                }
                break;
            case SO_ERROR:
                if (*optlen >= sizeof(int)) {
                    *(int *)optval = 0;
                    *optlen = sizeof(int);
                    return 0;
                }
                break;
            case SO_REUSEADDR:
                if (*optlen >= sizeof(int)) {
                    *(int *)optval = (sock->options & SO_REUSEADDR) ? 1 : 0;
                    *optlen = sizeof(int);
                    return 0;
                }
                break;
            default:
                serial_print("[socket_ops] Unknown socket option\n");
                return -1;
        }
    }
    
    return -1;
}

/* ============================================================================
 * Socket Info (REAL)
 * ============================================================================ */

int socket_getsockname(int fd, struct sockaddr *addr, uint32_t *addrlen)
{
    struct socket *sock = socket_find(fd);
    if (!sock) {
        serial_print("[socket_ops] Socket not found\n");
        return -1;
    }
    
    if (!addr || !addrlen) {
        return -1;
    }
    
    if (*addrlen < sizeof(struct sockaddr_in)) {
        return -1;
    }
    
    struct sockaddr_in *sin = (struct sockaddr_in *)addr;
    sin->sin_family = AF_INET;
    sin->sin_port = htons(sock->local_port);
    sin->sin_addr = ip_addr_to_u32(&sock->local_ip);
    *addrlen = sizeof(struct sockaddr_in);
    
    return 0;
}

int socket_getpeername(int fd, struct sockaddr *addr, uint32_t *addrlen)
{
    struct socket *sock = socket_find(fd);
    if (!sock) {
        serial_print("[socket_ops] Socket not found\n");
        return -1;
    }
    
    if (!addr || !addrlen) {
        return -1;
    }
    
    if (*addrlen < sizeof(struct sockaddr_in)) {
        return -1;
    }
    
    if (sock->state != SOCK_STATE_CONNECTED) {
        serial_print("[socket_ops] Socket not connected\n");
        return -1;
    }
    
    struct sockaddr_in *sin = (struct sockaddr_in *)addr;
    sin->sin_family = AF_INET;
    sin->sin_port = htons(sock->remote_port);
    sin->sin_addr = ip_addr_to_u32(&sock->remote_ip);
    *addrlen = sizeof(struct sockaddr_in);
    
    return 0;
}

/* ============================================================================
 * Socket IOCTL (REAL)
 * ============================================================================ */

int socket_ioctl(int fd, uint32_t cmd, void *arg)
{
    struct socket *sock = socket_find(fd);
    if (!sock) {
        serial_print("[socket_ops] Socket not found\n");
        return -1;
    }
    
    switch(cmd) {
        case 0x5421: /* FIONREAD - get bytes available to read */
            if (arg) {
                uint32_t *bytes = (uint32_t *)arg;
                *bytes = sock->recv_queue_len;
                return 0;
            }
            break;
        default:
            serial_print("[socket_ops] Unknown ioctl command\n");
            return -1;
    }
    
    return -1;
}