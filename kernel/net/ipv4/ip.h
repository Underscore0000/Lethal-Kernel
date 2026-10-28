/*
 * Lethal Kernel - IPv4 Header
 *
 * v5.0: IPv4 protocol header.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef LETHAL_NET_IPV4_IP_H
#define LETHAL_NET_IPV4_IP_H

#include <stdint.h>
#include <stddef.h>

#include "../core/net.h"
#include "../core/sk_buff.h"

/* ============================================================================
 * IPv4 Constants
 * ============================================================================ */

#define IP_HEADER_SIZE 20

#define IP_FLAG_DF 0x4000
#define IP_FLAG_MF 0x2000

/* IP Protocol Numbers */
#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

/* ============================================================================
 * IPv4 Header
 * ============================================================================ */

/*
 * Standard IPv4 header.
 *
 * version_ihl:
 *   high nibble = version
 *   low nibble  = Internet Header Length
 *
 * For a standard 20-byte IPv4 header:
 *
 *   version = 4
 *   IHL     = 5
 *   version_ihl = 0x45
 */
struct ip_header {
    uint8_t version_ihl;
    uint8_t tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed));

/* ============================================================================
 * Initialization
 * ============================================================================ */

int ip_init(void);
int ip_shutdown(void);

/* ============================================================================
 * IP Address Management
 * ============================================================================ */

void ip_set_local(ip_addr_t *ip);
void ip_get_local(ip_addr_t *ip);

/* ============================================================================
 * Packet Operations
 * ============================================================================ */

int ip_input(struct sk_buff *skb);

int ip_output(struct sk_buff *skb,
              ip_addr_t *src,
              ip_addr_t *dst,
              uint8_t protocol);

int ip_receive(struct sk_buff *skb);

int ip_send(struct sk_buff *skb,
            ip_addr_t *src,
            ip_addr_t *dst,
            uint8_t protocol);

int ip_forward(struct sk_buff *skb);

/* ============================================================================
 * Header Operations
 * ============================================================================ */

int ip_build_header(struct ip_header *hdr,
                    ip_addr_t *src,
                    ip_addr_t *dst,
                    uint8_t protocol,
                    uint16_t len);

int ip_parse_header(struct ip_header *hdr,
                    ip_addr_t *src,
                    ip_addr_t *dst,
                    uint8_t *protocol,
                    uint16_t *len);

/*
 * IPv4 Internet checksum.
 *
 * The function operates on raw bytes so it is safe to use with packed
 * network headers that may not be naturally aligned.
 */
uint16_t ip_checksum(const void *data, size_t len);

/* ============================================================================
 * Routing
 * ============================================================================ */

int ip_route(ip_addr_t *dest, ip_addr_t *next_hop);

int ip_add_route(uint32_t dest,
                 uint32_t netmask,
                 uint32_t gateway,
                 uint32_t dev_id);

#endif /* LETHAL_NET_IPV4_IP_H */