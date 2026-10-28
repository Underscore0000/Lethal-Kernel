/*
 * Lethal Kernel - TCP Implementation
 *
 * v5.0: Transmission Control Protocol implementation.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "tcp.h"
#include "../core/net.h"
#include "../core/device.h"
#include "../core/sk_buff.h"
#include "../ipv4/ip.h"
#include <net/socket.h>
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>
#include <stdlib.h>

/* TCP configuration */
#define TCP_MAX_SOCKETS 64

/* TCP socket table */
static struct tcp_socket tcp_sockets[TCP_MAX_SOCKETS];
static uint8_t tcp_initialized = 0;
static uint16_t tcp_port_counter = 1024;

/* Forward declarations */
int tcp_close_fd(int fd);

/* ============================================================================
 * Initialization
 * ============================================================================ */

int tcp_init(void)
{
    serial_print("[tcp] Initializing...\n");
    
    if (tcp_initialized) {
        serial_print("[tcp] Already initialized\n");
        return 0;
    }
    
    memset(tcp_sockets, 0, sizeof(tcp_sockets));
    tcp_port_counter = 1024;
    
    tcp_initialized = 1;
    
    serial_print("[tcp] Initialized\n");
    return 0;
}

int tcp_shutdown(void)
{
    serial_print("[tcp] Shutting down...\n");
    
    /* Close all sockets */
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (tcp_sockets[i].in_use) {
            tcp_close_fd((int)tcp_sockets[i].fd);
        }
    }
    
    tcp_initialized = 0;
    
    serial_print("[tcp] Shutdown complete\n");
    return 0;
}

/* ============================================================================
 * TCP Socket Management
 * ============================================================================ */

struct tcp_socket *tcp_socket_alloc(void)
{
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (!tcp_sockets[i].in_use) {
            memset(&tcp_sockets[i], 0, sizeof(struct tcp_socket));
            tcp_sockets[i].in_use = 1;
            tcp_sockets[i].state = TCP_STATE_CLOSED;
            tcp_sockets[i].fd = i + 3;
            tcp_sockets[i].src_port = ++tcp_port_counter;
            return &tcp_sockets[i];
        }
    }
    return NULL;
}

int tcp_socket_free(struct tcp_socket *sock)
{
    if (!sock) return -1;
    
    sock->in_use = 0;
    sock->state = TCP_STATE_CLOSED;
    return 0;
}

struct tcp_socket *tcp_socket_find(uint16_t src_port, uint16_t dst_port, uint32_t src_ip, uint32_t dst_ip)
{
    (void)src_ip;
    (void)dst_ip;
    
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (tcp_sockets[i].in_use &&
            tcp_sockets[i].src_port == src_port &&
            tcp_sockets[i].dst_port == dst_port) {
            return &tcp_sockets[i];
        }
    }
    return NULL;
}

struct tcp_socket *tcp_socket_find_by_port(uint16_t port)
{
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (tcp_sockets[i].in_use && tcp_sockets[i].src_port == port) {
            return &tcp_sockets[i];
        }
    }
    return NULL;
}

struct tcp_socket *tcp_socket_find_by_fd(int fd)
{
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (tcp_sockets[i].in_use && (int)tcp_sockets[i].fd == fd) {
            return &tcp_sockets[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * TCP Connect Functions
 * ============================================================================ */

int tcp_connect(int fd, void *addr, uint32_t addrlen)
{
    struct tcp_socket *sock = tcp_socket_find_by_fd(fd);
    if (!sock) return -1;
    
    (void)addrlen;
    
    struct sockaddr_in *sin = (struct sockaddr_in *)addr;
    uint32_t dest_ip = sin->sin_addr;
    uint16_t dest_port = ntohs(sin->sin_port);
    
    serial_print("[tcp] Connecting to ");
    serial_print_dec((dest_ip >> 24) & 0xFF);
    serial_print(".");
    serial_print_dec((dest_ip >> 16) & 0xFF);
    serial_print(".");
    serial_print_dec((dest_ip >> 8) & 0xFF);
    serial_print(".");
    serial_print_dec(dest_ip & 0xFF);
    serial_print(":");
    serial_print_dec(dest_port);
    serial_print("\n");
    
    sock->dst_port = dest_port;
    sock->state = TCP_STATE_SYN_SENT;
    sock->seq_num = rand();
    sock->ack_num = 0;
    
    /* Invia SYN */
    tcp_send_syn(sock);
    
    return 0;
}

int tcp_close_fd(int fd)
{
    struct tcp_socket *sock = tcp_socket_find_by_fd(fd);
    if (!sock) return -1;
    
    serial_print("[tcp] Closing socket\n");
    
    if (sock->state == TCP_STATE_ESTABLISHED ||
        sock->state == TCP_STATE_SYN_RECV) {
        tcp_send_fin(sock);
        sock->state = TCP_STATE_FIN_WAIT1;
    }
    
    tcp_socket_free(sock);
    return 0;
}

/* ============================================================================
 * TCP Socket Operations (compatibili con syscall)
 * ============================================================================ */

int tcp_socket(int domain, int type, int protocol)
{
    (void)domain;
    (void)type;
    (void)protocol;
    
    struct tcp_socket *sock = tcp_socket_alloc();
    if (!sock) return -1;
    
    sock->state = TCP_STATE_CLOSED;
    return (int)sock->fd;
}

int tcp_bind(int fd, void *addr, uint32_t addrlen)
{
    struct tcp_socket *sock = tcp_socket_find_by_fd(fd);
    if (!sock) return -1;
    
    (void)addrlen;
    
    struct sockaddr_in *sin = (struct sockaddr_in *)addr;
    sock->src_port = ntohs(sin->sin_port);
    
    sock->state = TCP_STATE_LISTEN;
    return 0;
}

int tcp_listen_fd(int fd, int backlog)
{
    struct tcp_socket *sock = tcp_socket_find_by_fd(fd);
    if (!sock) return -1;
    
    (void)backlog;
    
    sock->state = TCP_STATE_LISTEN;
    return 0;
}

int tcp_accept_fd(int fd, void *addr, uint32_t *addrlen)
{
    struct tcp_socket *sock = tcp_socket_find_by_fd(fd);
    if (!sock || sock->state != TCP_STATE_LISTEN) return -1;
    
    (void)addr;
    (void)addrlen;
    
    serial_print("[tcp] Accept not implemented\n");
    return -1;
}

/* ============================================================================
 * TCP Send/Recv (compatibili con syscall)
 * ============================================================================ */

int tcp_send_fd(int fd, const void *buf, size_t len, int flags)
{
    struct tcp_socket *sock = tcp_socket_find_by_fd(fd);
    if (!sock) return -1;
    
    (void)buf;
    (void)flags;
    
    if (sock->state != TCP_STATE_ESTABLISHED) {
        serial_print("[tcp] Socket not established\n");
        return -1;
    }
    
    serial_print("[tcp] Sending ");
    serial_print_dec(len);
    serial_print(" bytes\n");
    
    return (int)len;
}

int tcp_recv_fd(int fd, void *buf, size_t len, int flags)
{
    struct tcp_socket *sock = tcp_socket_find_by_fd(fd);
    if (!sock) return -1;
    
    (void)buf;
    (void)flags;
    (void)len;
    
    if (sock->state != TCP_STATE_ESTABLISHED) {
        return -1;
    }
    
    return 0;
}

/* ============================================================================
 * TCP Process Packet
 * ============================================================================ */

int tcp_process_packet(struct sk_buff *skb)
{
    if (!skb) return -1;
    
    struct tcp_header *tcp = (struct tcp_header *)skb->data;
    uint16_t src_port = ntohs(tcp->src_port);
    uint16_t dst_port = ntohs(tcp->dst_port);
    uint32_t seq = ntohl(tcp->seq_num);
    uint32_t ack = ntohl(tcp->ack_num);
    uint8_t flags = tcp->flags;
    
    serial_print("[tcp] Received packet: ");
    serial_print_dec(src_port);
    serial_print(" -> ");
    serial_print_dec(dst_port);
    serial_print("\n");
    

    struct tcp_socket *sock = tcp_socket_find(dst_port, src_port, 0, 0);
    
    if (!sock) {
        if (flags & TCP_FLAG_SYN) {
            serial_print("[tcp] No socket for SYN\n");
        }
        return -1;
    }
    
    sock->dst_port = src_port;
    sock->ack_num = ack;
    

    if (flags & TCP_FLAG_RST) {
        sock->state = TCP_STATE_CLOSED;
        tcp_socket_free(sock);
        return 0;
    }
    
    if (flags & TCP_FLAG_SYN) {
        if (sock->state == TCP_STATE_LISTEN) {
            sock->seq_num = rand();
            sock->ack_num = seq + 1;
            sock->state = TCP_STATE_SYN_RECV;
            tcp_send_syn_ack(sock);
        }
        return 0;
    }
    
    if (flags & TCP_FLAG_ACK) {
        if (sock->state == TCP_STATE_SYN_SENT) {
            sock->state = TCP_STATE_ESTABLISHED;
            serial_print("[tcp] Connection established\n");
        }
        if (sock->state == TCP_STATE_SYN_RECV) {
            sock->state = TCP_STATE_ESTABLISHED;
            serial_print("[tcp] Connection established\n");
        }
    }
    
    if (flags & TCP_FLAG_FIN) {
        sock->state = TCP_STATE_CLOSE_WAIT;
        serial_print("[tcp] FIN received\n");
        tcp_send_ack(sock);
        tcp_socket_free(sock);
        return 0;
    }
    
    return 0;
}

/* ============================================================================
 * TCP Packet Construction (stub)
 * ============================================================================ */

int tcp_send_packet(struct sk_buff *skb, ip_addr_t *src, uint16_t src_port,
                     ip_addr_t *dst, uint16_t dst_port, uint8_t flags)
{
    (void)skb;
    (void)src;
    (void)dst;
    (void)flags;
    
    serial_print("[tcp] Send packet: ");
    serial_print_dec(src_port);
    serial_print(" -> ");
    serial_print_dec(dst_port);
    serial_print("\n");
    
    return 0;
}

/* ============================================================================
 * TCP Utility Functions (weak definitions)
 * ============================================================================ */

__attribute__((weak)) int tcp_send_segment(struct tcp_socket *sock, uint8_t *data, size_t len, uint8_t flags)
{
    (void)sock;
    (void)data;
    (void)len;
    (void)flags;
    serial_print("[tcp] Send segment (weak)\n");
    return 0;
}

__attribute__((weak)) int tcp_send_syn(struct tcp_socket *sock)
{
    (void)sock;
    serial_print("[tcp] Send SYN (weak)\n");
    return 0;
}

__attribute__((weak)) int tcp_send_syn_ack(struct tcp_socket *sock)
{
    (void)sock;
    serial_print("[tcp] Send SYN-ACK (weak)\n");
    return 0;
}

__attribute__((weak)) int tcp_send_ack(struct tcp_socket *sock)
{
    (void)sock;
    serial_print("[tcp] Send ACK (weak)\n");
    return 0;
}

__attribute__((weak)) int tcp_send_data(struct tcp_socket *sock, const void *data, size_t len)
{
    (void)sock;
    (void)data;
    (void)len;
    serial_print("[tcp] Send DATA (weak)\n");
    return 0;
}

__attribute__((weak)) int tcp_send_fin(struct tcp_socket *sock)
{
    (void)sock;
    serial_print("[tcp] Send FIN (weak)\n");
    return 0;
}

__attribute__((weak)) int tcp_send_rst(struct tcp_socket *sock)
{
    (void)sock;
    serial_print("[tcp] Send RST (weak)\n");
    return 0;
}

/* Timer functions */
__attribute__((weak)) void tcp_timer_init(void)
{
    serial_print("[tcp] Timer init (weak)\n");
}

__attribute__((weak)) void tcp_timer_start(struct tcp_socket *sock, uint32_t timeout_ms)
{
    (void)sock;
    (void)timeout_ms;
    serial_print("[tcp] Timer start (weak)\n");
}

__attribute__((weak)) void tcp_timer_stop(struct tcp_socket *sock)
{
    (void)sock;
    serial_print("[tcp] Timer stop (weak)\n");
}

__attribute__((weak)) void tcp_timer_tick(void)
{
    serial_print("[tcp] Timer tick (weak)\n");
}

__attribute__((weak)) void tcp_timer_handle_retransmit(struct tcp_socket *sock)
{
    (void)sock;
    serial_print("[tcp] Retransmit (weak)\n");
}

__attribute__((weak)) void tcp_timer_cleanup(struct tcp_socket *sock)
{
    (void)sock;
    serial_print("[tcp] Timer cleanup (weak)\n");
}