/*
 * Lethal Kernel - UDP Header
 *
 * v5.0: UDP protocol structures and constants.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef LETHAL_NET_IPV4_UDP_H
#define LETHAL_NET_IPV4_UDP_H

#include <stdint.h>
#include <stddef.h>

#include "../core/sk_buff.h"

/* ============================================================================
 * UDP Header Structure
 * ============================================================================ */

struct udp_header {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed));

/* ============================================================================
 * UDP Socket Structure
 * ============================================================================ */

struct udp_socket {
    uint32_t fd;

    uint16_t src_port;
    ip_addr_t src_ip;

    ip_addr_t dst_ip;
    uint16_t dst_port;

    uint8_t bound;
    uint8_t connected;
    uint8_t in_use;

    void *next;
};

/* ============================================================================
 * UDP API
 * ============================================================================ */

int udp_init(void);

int udp_send(struct sk_buff *skb,
             ip_addr_t *src,
             uint16_t src_port,
             ip_addr_t *dst,
             uint16_t dst_port);

int udp_receive(struct sk_buff *skb);

int udp_socket_create(void);

int udp_socket_bind(int fd, uint16_t port);

int udp_socket_sendto(int fd,
                      const void *data,
                      size_t len,
                      ip_addr_t *dst,
                      uint16_t port);

#endif /* LETHAL_NET_IPV4_UDP_H */
