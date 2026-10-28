/*
 * Lethal Kernel - Socket Implementation
 *
 * v5.0: Simplified BSD socket implementation.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "socket.h"
#include <net/socket.h>
#include "../core/device.h"
#include "../ipv4/udp.h"
#include "../ipv4/tcp.h"
#include "../config/privacy.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>
#include <stdlib.h>

#define SOCKET_MAX 256

static struct socket sockets[SOCKET_MAX];
static uint32_t next_fd = 3;
static uint8_t socket_initialized = 0;

/* Silenzia il warning per sockets array non usato direttamente */
__attribute__((unused)) static struct socket *socket_get_array(void) { return sockets; }

/* ============================================================================
 * Initialization
 * ============================================================================ */

int socket_init(void)
{
    serial_print("[socket] Initializing...\n");
    
    if (socket_initialized) {
        serial_print("[socket] Already initialized\n");
        return 0;
    }
    
    memset(sockets, 0, sizeof(sockets));
    next_fd = 3;
    socket_initialized = 1;
    
    serial_print("[socket] Initialized\n");
    return 0;
}

/* ============================================================================
 * Socket Management
 * ============================================================================ */

struct socket *socket_alloc(void)
{
    for (int i = 0; i < SOCKET_MAX; i++) {
        if (!sockets[i].in_use) {
            memset(&sockets[i], 0, sizeof(struct socket));
            sockets[i].fd = next_fd++;
            sockets[i].in_use = 1;
            sockets[i].state = SOCK_STATE_OPEN;
            return &sockets[i];
        }
    }
    return NULL;
}

int socket_free(struct socket *sock)
{
    if (!sock) return -1;
    
    struct sk_buff *skb = sock->recv_queue;
    while (skb) {
        struct sk_buff *next = (struct sk_buff *)skb->next;
        skb_free(skb);
        skb = next;
    }
    
    sock->in_use = 0;
    sock->state = SOCK_STATE_CLOSED;
    return 0;
}

struct socket *socket_find(uint32_t fd)
{
    int fd_int = (int)fd;
    for (int i = 0; i < SOCKET_MAX; i++) {
        if (sockets[i].in_use && (int)sockets[i].fd == fd_int) {
            return &sockets[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Socket Creation
 * ============================================================================ */

int socket_create(int domain, int type, int protocol)
{
    serial_print("[socket] Creating socket\n");
    
    if (domain != AF_INET) {
        serial_print("[socket] Unsupported domain\n");
        return -1;
    }
    
    struct socket *sock = socket_alloc();
    if (!sock) {
        serial_print("[socket] Failed to allocate socket\n");
        return -1;
    }
    
    sock->domain = domain;
    sock->type = type;
    sock->protocol = protocol;
    
    switch(type) {
        case SOCK_STREAM:
            sock->protocol = IPPROTO_TCP;
            sock->state = SOCK_STATE_OPEN;
            serial_print("[socket] Created TCP socket\n");
            return (int)sock->fd;
            
        case SOCK_DGRAM:
            sock->protocol = IPPROTO_UDP;
            sock->state = SOCK_STATE_OPEN;
            serial_print("[socket] Created UDP socket\n");
            return (int)sock->fd;
            
        default:
            serial_print("[socket] Unsupported socket type\n");
            socket_free(sock);
            return -1;
    }
}

/* ============================================================================
 * Socket Bind (simplified)
 * ============================================================================ */

int socket_bind(int fd, const void *addr, uint32_t addrlen)
{
    (void)addr;
    (void)addrlen;
    
    struct socket *sock = socket_find((uint32_t)fd);
    if (!sock) {
        serial_print("[socket] Socket not found\n");
        return -1;
    }
    
    serial_print("[socket] Bind called on fd ");
    serial_print_dec(fd);
    serial_print("\n");
    
    sock->state = SOCK_STATE_BOUND;
    return 0;
}

/* ============================================================================
 * Socket Connect (simplified)
 * ============================================================================ */

int socket_connect(int fd, const void *addr, uint32_t addrlen)
{
    (void)addr;
    (void)addrlen;
    
    struct socket *sock = socket_find((uint32_t)fd);
    if (!sock) {
        serial_print("[socket] Socket not found\n");
        return -1;
    }
    
    serial_print("[socket] Connect called on fd ");
    serial_print_dec(fd);
    serial_print("\n");
    
    sock->state = SOCK_STATE_CONNECTED;
    return 0;
}

/* ============================================================================
 * Socket Listen (simplified)
 * ============================================================================ */

int socket_listen(int fd, int backlog)
{
    (void)backlog;
    
    struct socket *sock = socket_find((uint32_t)fd);
    if (!sock) {
        serial_print("[socket] Socket not found\n");
        return -1;
    }
    
    serial_print("[socket] Listen called on fd ");
    serial_print_dec(fd);
    serial_print("\n");
    
    sock->state = SOCK_STATE_LISTENING;
    return 0;
}

/* ============================================================================
 * Socket Accept (simplified)
 * ============================================================================ */

int socket_accept(int fd, void *addr, uint32_t *addrlen)
{
    (void)addr;
    (void)addrlen;
    
    struct socket *sock = socket_find((uint32_t)fd);
    if (!sock) {
        serial_print("[socket] Socket not found\n");
        return -1;
    }
    
    serial_print("[socket] Accept called on fd ");
    serial_print_dec(fd);
    serial_print("\n");
    
    int new_fd = (int)next_fd++;
    return new_fd;
}

/* ============================================================================
 * Socket Send (simplified)
 * ============================================================================ */

int socket_send(int fd, const void *buf, size_t len, int flags)
{
    (void)buf;
    (void)flags;
    
    struct socket *sock = socket_find((uint32_t)fd);
    if (!sock) {
        serial_print("[socket] Socket not found\n");
        return -1;
    }
    
    serial_print("[socket] Send on fd ");
    serial_print_dec(fd);
    serial_print(" len ");
    serial_print_dec(len);
    serial_print("\n");
    
    return (int)len;
}

/* ============================================================================
 * Socket Recv (simplified)
 * ============================================================================ */

int socket_recv(int fd, void *buf, size_t len, int flags)
{
    (void)buf;
    (void)flags;
    (void)len;
    
    struct socket *sock = socket_find((uint32_t)fd);
    if (!sock) {
        serial_print("[socket] Socket not found\n");
        return -1;
    }
    
    serial_print("[socket] Recv on fd ");
    serial_print_dec(fd);
    serial_print("\n");
    
    return 0;
}

/* ============================================================================
 * Socket Sendto (simplified)
 * ============================================================================ */

int socket_sendto(int fd, const void *buf, size_t len, int flags,
                   const void *dest_addr, uint32_t addrlen)
{
    (void)buf;
    (void)flags;
    (void)dest_addr;
    (void)addrlen;
    
    struct socket *sock = socket_find((uint32_t)fd);
    if (!sock) {
        serial_print("[socket] Socket not found\n");
        return -1;
    }
    
    serial_print("[socket] Sendto on fd ");
    serial_print_dec(fd);
    serial_print(" len ");
    serial_print_dec(len);
    serial_print("\n");
    
    return (int)len;
}

/* ============================================================================
 * Socket Recvfrom (simplified)
 * ============================================================================ */

int socket_recvfrom(int fd, void *buf, size_t len, int flags,
                     void *src_addr, uint32_t *addrlen)
{
    (void)buf;
    (void)flags;
    (void)src_addr;
    (void)addrlen;
    (void)len;
    
    struct socket *sock = socket_find((uint32_t)fd);
    if (!sock) {
        serial_print("[socket] Socket not found\n");
        return -1;
    }
    
    serial_print("[socket] Recvfrom on fd ");
    serial_print_dec(fd);
    serial_print("\n");
    
    return 0;
}

/* ============================================================================
 * Socket Close
 * ============================================================================ */

int socket_close(int fd)
{
    struct socket *sock = socket_find((uint32_t)fd);
    if (!sock) {
        serial_print("[socket] Socket not found\n");
        return -1;
    }
    
    socket_free(sock);
    serial_print("[socket] Closed fd=");
    serial_print_dec(fd);
    serial_print("\n");
    return 0;
}

/* ============================================================================
 * Internal Queue Functions
 * ============================================================================ */

int socket_deliver(struct socket *sock, struct sk_buff *skb)
{
    if (!sock || !skb) return -1;
    
    skb->next = NULL;
    skb->prev = NULL;
    
    if (sock->recv_queue == NULL) {
        sock->recv_queue = skb;
    } else {
        struct sk_buff *last = sock->recv_queue;
        while (last->next) {
            last = (struct sk_buff *)last->next;
        }
        last->next = skb;
        skb->prev = last;
    }
    
    sock->recv_queue_len++;
    return 0;
}

struct sk_buff *socket_dequeue(struct socket *sock)
{
    if (!sock || !sock->recv_queue) return NULL;
    
    struct sk_buff *skb = sock->recv_queue;
    sock->recv_queue = (struct sk_buff *)skb->next;
    if (sock->recv_queue) {
        sock->recv_queue->prev = NULL;
    }
    sock->recv_queue_len--;
    skb->next = NULL;
    skb->prev = NULL;
    
    return skb;
}