/*
 * Lethal Kernel - TCP Header
 *
 * v5.0: TCP protocol structures, constants, and function prototypes.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_IPV4_TCP_H
#define LETHAL_NET_IPV4_TCP_H

#include <stdint.h>
#include "../core/net.h"
#include "../core/sk_buff.h"

/* ============================================================================
 * TCP Flags
 * ============================================================================ */

#define TCP_FLAG_FIN  (1 << 0)
#define TCP_FLAG_SYN  (1 << 1)
#define TCP_FLAG_RST  (1 << 2)
#define TCP_FLAG_PSH  (1 << 3)
#define TCP_FLAG_ACK  (1 << 4)
#define TCP_FLAG_URG  (1 << 5)
#define TCP_FLAG_ECE  (1 << 6)
#define TCP_FLAG_CWR  (1 << 7)

/* ============================================================================
 * TCP States
 * ============================================================================ */

#define TCP_STATE_CLOSED      0
#define TCP_STATE_LISTEN      1
#define TCP_STATE_SYN_SENT    2
#define TCP_STATE_SYN_RECV    3
#define TCP_STATE_ESTABLISHED 4
#define TCP_STATE_FIN_WAIT1   5
#define TCP_STATE_FIN_WAIT2   6
#define TCP_STATE_CLOSE_WAIT  7
#define TCP_STATE_CLOSING     8
#define TCP_STATE_LAST_ACK    9
#define TCP_STATE_TIME_WAIT   10

/* ============================================================================
 * TCP Header Structure
 * ============================================================================ */

struct tcp_header {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t data_offset : 4;
    uint8_t reserved : 3;
    uint8_t ns : 1;
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} __attribute__((packed));

/* ============================================================================
 * TCP Socket Structure
 * ============================================================================ */

struct tcp_socket {
    uint32_t fd;
    uint16_t src_port;
    uint16_t dst_port;
    ip_addr_t src_ip;
    ip_addr_t dst_ip;
    uint32_t state;
    uint32_t seq_num;
    uint32_t ack_num;
    uint32_t window;
    uint32_t rcv_next;
    uint32_t snd_next;
    uint32_t snd_una;
    uint32_t snd_wnd;
    uint32_t rcv_wnd;
    uint8_t bound;
    uint8_t connected;
    uint8_t in_use;
    uint8_t syn_received;
    uint8_t fin_received;
    uint8_t fin_sent;
    void *next;
};

/* ============================================================================
 * TCP Functions
 * ============================================================================ */

int tcp_send(struct sk_buff *skb, ip_addr_t *src, uint16_t src_port,
             ip_addr_t *dst, uint16_t dst_port, uint8_t flags);
int tcp_receive(struct sk_buff *skb);
int tcp_connect(int fd, void *addr, uint32_t addrlen);
int tcp_socket_create(void);
int tcp_socket_bind(int fd, uint16_t port);
int tcp_socket_listen(int fd, int backlog);
int tcp_socket_accept(int fd, void *addr, uint32_t *addrlen);
int tcp_socket_close(int fd);

/* ============================================================================
 * TCP Output Functions
 * ============================================================================ */

int tcp_send_segment(struct tcp_socket *sock, uint8_t *data, size_t len, uint8_t flags);
int tcp_send_syn(struct tcp_socket *sock);
int tcp_send_syn_ack(struct tcp_socket *sock);
int tcp_send_ack(struct tcp_socket *sock);
int tcp_send_data(struct tcp_socket *sock, const void *data, size_t len);
int tcp_send_fin(struct tcp_socket *sock);
int tcp_send_rst(struct tcp_socket *sock);

/* ============================================================================
 * TCP Timer Functions
 * ============================================================================ */

void tcp_timer_init(void);
void tcp_timer_start(struct tcp_socket *sock, uint32_t timeout_ms);
void tcp_timer_stop(struct tcp_socket *sock);
void tcp_timer_tick(void);
void tcp_timer_handle_retransmit(struct tcp_socket *sock);
void tcp_timer_cleanup(struct tcp_socket *sock);

#endif /* LETHAL_NET_IPV4_TCP_H */