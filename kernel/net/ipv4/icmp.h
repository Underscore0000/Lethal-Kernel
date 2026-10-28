/*
 * Lethal Kernel - ICMP Header
 *
 * v5.0: ICMP protocol structures and constants.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_IPV4_ICMP_H
#define LETHAL_NET_IPV4_ICMP_H

#include <stdint.h>
#include "../core/sk_buff.h"

/* ============================================================================
 * ICMP Types
 * ============================================================================ */

#define ICMP_TYPE_ECHO_REPLY       0
#define ICMP_TYPE_DEST_UNREACHABLE 3
#define ICMP_TYPE_SOURCE_QUENCH    4
#define ICMP_TYPE_REDIRECT         5
#define ICMP_TYPE_ECHO_REQUEST     8
#define ICMP_TYPE_TIME_EXCEEDED    11
#define ICMP_TYPE_PARAM_PROBLEM    12
#define ICMP_TYPE_TIMESTAMP_REQUEST 13
#define ICMP_TYPE_TIMESTAMP_REPLY   14
#define ICMP_TYPE_INFO_REQUEST      15
#define ICMP_TYPE_INFO_REPLY        16

/* ============================================================================
 * ICMP Header Structure (without packed to avoid alignment issues)
 * ============================================================================ */

struct icmp_header {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    union {
        struct {
            uint16_t id;
            uint16_t sequence;
        } echo;
        struct {
            uint16_t unused;
            uint16_t mtu;
        } frag;
        uint32_t gateway;
    } data;
};

/* ============================================================================
 * Function Prototypes
 * ============================================================================ */

int icmp_send_echo_reply(ip_addr_t *src, ip_addr_t *dst, uint16_t id, uint16_t seq);
int icmp_send_echo_request(ip_addr_t *dst, uint16_t id, uint16_t seq);
int icmp_receive(struct sk_buff *skb);
int icmp_handle_echo_request(struct sk_buff *skb);
/* Aggiungi in icmp.h */
int icmp_init(void);

#endif /* LETHAL_NET_IPV4_ICMP_H */