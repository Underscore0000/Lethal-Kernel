/*
 * Lethal Kernel - IPv4 Implementation
 *
 * v5.0: IPv4 protocol with privacy features.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */

#include "ip.h"
#include "icmp.h"
#include "udp.h"
#include "tcp.h"
#include "arp.h"

#include "../core/device.h"
#include "../core/ethernet.h"
#include "../config/privacy.h"

#include "../../core/console.h"
#include "../../memory/heap.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define ROUTE_TABLE_SIZE 16
#define ETH_HEADER_SIZE  14

#define IP_VERSION       4
#define IP_IHL_MIN       5
#define IP_IHL_DEFAULT   5
#define IP_TTL_DEFAULT   64

/* ============================================================================
 * Routing Table
 * ============================================================================ */

static struct route_entry {
    uint32_t dest;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dev_id;
    uint8_t valid;
} route_table[ROUTE_TABLE_SIZE];

static uint32_t route_count = 0;

/* ============================================================================
 * IPv4 State
 * ============================================================================ */

static ip_addr_t ip_local_addr;
static uint8_t ip_initialized = 0;

/* ============================================================================
 * Byte Order Helpers
 * ============================================================================ */

static uint16_t ip_htons(uint16_t value)
{
    return (uint16_t)((value << 8) | (value >> 8));
}

static uint16_t ip_ntohs(uint16_t value)
{
    return ip_htons(value);
}

/* ============================================================================
 * IPv4 Checksum
 * ============================================================================ */

/*
 * RFC 1071 Internet checksum.
 *
 * We intentionally read the packet byte-by-byte.
 *
 * struct ip_header is packed, therefore casting it to uint16_t * can produce
 * an unaligned pointer and triggers -Werror=address-of-packed-member.
 */
uint16_t ip_checksum(const void *data, size_t len)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t sum = 0;

    if (!data)
        return 0;

    while (len >= 2U) {
        uint16_t word;

        word = (uint16_t)(((uint16_t)bytes[0] << 8) |
                          (uint16_t)bytes[1]);

        sum += word;

        bytes += 2;
        len -= 2;
    }

    if (len != 0U) {
        sum += (uint16_t)((uint16_t)bytes[0] << 8);
    }

    while ((sum >> 16) != 0U) {
        sum = (sum & 0xFFFFU) + (sum >> 16);
    }

    return (uint16_t)~sum;
}

/* ============================================================================
 * Header Operations
 * ============================================================================ */

int ip_build_header(struct ip_header *hdr,
                    ip_addr_t *src,
                    ip_addr_t *dst,
                    uint8_t protocol,
                    uint16_t len)
{
    if (!hdr || !src || !dst)
        return -1;

    /*
     * Standard IPv4 header:
     *
     * version = 4
     * IHL     = 5 (20 bytes)
     *
     * 0x45 = 0100 0101
     */
    hdr->version_ihl = (uint8_t)((IP_VERSION << 4) | IP_IHL_DEFAULT);

    hdr->tos = 0;

    /*
     * len is the payload length. Add the 20-byte IPv4 header.
     */
    hdr->total_len = ip_htons((uint16_t)(len + IP_HEADER_SIZE));

    /*
     * Identification field.
     */
    hdr->id = ip_htons((uint16_t)(rand() & 0xFFFF));

    /*
     * Don't Fragment.
     */
    hdr->flags_frag = ip_htons(IP_FLAG_DF);

    hdr->ttl = IP_TTL_DEFAULT;
    hdr->protocol = protocol;

    /*
     * Addresses are represented by ip_addr_t / the networking layer.
     */
    hdr->src_ip = ip_addr_to_u32(src);
    hdr->dst_ip = ip_addr_to_u32(dst);

    /*
     * Checksum must be zero while calculating the checksum.
     */
    hdr->checksum = 0;

    hdr->checksum = ip_checksum(hdr, IP_HEADER_SIZE);

    return 0;
}

/* ============================================================================
 * Parse IPv4 Header
 * ============================================================================ */

int ip_parse_header(struct ip_header *hdr,
                    ip_addr_t *src,
                    ip_addr_t *dst,
                    uint8_t *protocol,
                    uint16_t *len)
{
    uint8_t version;
    uint8_t ihl;
    uint16_t received_checksum;
    uint16_t calculated_checksum;
    uint16_t total_length;

    if (!hdr)
        return -1;

    /*
     * Extract IPv4 version and IHL.
     */
    version = (uint8_t)(hdr->version_ihl >> 4);
    ihl = (uint8_t)(hdr->version_ihl & 0x0F);

    /*
     * Only IPv4 is supported here.
     */
    if (version != IP_VERSION) {
        serial_print("[ip] Invalid IP version\n");
        return -1;
    }

    /*
     * Minimum IPv4 IHL is 5 words = 20 bytes.
     */
    if (ihl < IP_IHL_MIN) {
        serial_print("[ip] Invalid IP header length\n");
        return -1;
    }

    /*
     * This implementation currently supports only a 20-byte IPv4 header.
     * Options can be added later if required.
     */
    if (ihl != IP_IHL_DEFAULT) {
        serial_print("[ip] IP options not supported\n");
        return -1;
    }

    /*
     * Verify header checksum.
     */
    received_checksum = hdr->checksum;

    hdr->checksum = 0;

    calculated_checksum = ip_checksum(hdr, IP_HEADER_SIZE);

    hdr->checksum = received_checksum;

    if (calculated_checksum != received_checksum) {
        serial_print("[ip] Invalid IP checksum\n");
        return -1;
    }

    /*
     * Convert total length from network byte order.
     */
    total_length = ip_ntohs(hdr->total_len);

    if (total_length < IP_HEADER_SIZE) {
        serial_print("[ip] Invalid IP total length\n");
        return -1;
    }

    if (src)
        ip_addr_from_u32(src, hdr->src_ip);

    if (dst)
        ip_addr_from_u32(dst, hdr->dst_ip);

    if (protocol)
        *protocol = hdr->protocol;

    if (len)
        *len = total_length;

    return 0;
}

/* ============================================================================
 * IP Address Management
 * ============================================================================ */

void ip_set_local(ip_addr_t *ip)
{
    if (!ip)
        return;

    memcpy(&ip_local_addr, ip, sizeof(ip_addr_t));

    serial_print("[ip] Local IP set to: ");

    serial_print_dec(ip->addr[0]);
    serial_print(".");
    serial_print_dec(ip->addr[1]);
    serial_print(".");
    serial_print_dec(ip->addr[2]);
    serial_print(".");
    serial_print_dec(ip->addr[3]);
    serial_print("\n");
}

void ip_get_local(ip_addr_t *ip)
{
    if (!ip)
        return;

    memcpy(ip, &ip_local_addr, sizeof(ip_addr_t));
}

/* ============================================================================
 * IP Send
 * ============================================================================ */

int ip_send(struct sk_buff *skb,
            ip_addr_t *src,
            ip_addr_t *dst,
            uint8_t protocol)
{
    struct net_device *dev;
    struct ip_header *ip_hdr;
    struct eth_header *eth_hdr;

    if (!skb || !src || !dst)
        return -1;

    dev = net_device_get(0);
    if (!dev)
        return -1;

    /*
     * Add IPv4 header at the front of the packet.
     */
    if (skb_push(skb, IP_HEADER_SIZE) != 0)
        return -1;

    ip_hdr = (struct ip_header *)skb->data;

    /*
     * skb->len includes the IPv4 header after skb_push().
     */
    if (ip_build_header(ip_hdr,
                        src,
                        dst,
                        protocol,
                        (uint16_t)(skb->len - IP_HEADER_SIZE)) != 0) {
        skb_pull(skb, IP_HEADER_SIZE);
        return -1;
    }

    /*
     * Add Ethernet header.
     */
    if (skb_push(skb, ETH_HEADER_SIZE) != 0) {
        skb_pull(skb, IP_HEADER_SIZE);
        return -1;
    }

    eth_hdr = (struct eth_header *)skb->data;

    /*
     * dev->mac is stored as a 6-byte array, while eth_build_header()
     * expects a mac_addr_t *.
     *
     * The underlying storage is the same six-byte MAC address.
     */
    eth_build_header(eth_hdr,
                     NULL,
                     (mac_addr_t *)&dev->mac,
                     ip_htons(ETH_P_IP));

    return net_device_transmit(dev, skb);
}

/* ============================================================================
 * IP Receive
 * ============================================================================ */

int ip_receive(struct sk_buff *skb)
{
    struct ip_header *ip_hdr;
    uint16_t total_length;
    uint8_t protocol;
    ip_addr_t src;
    ip_addr_t dst;

    if (!skb)
        return -1;

    if (!skb->data) {
        skb_free(skb);
        return -1;
    }

    /*
     * We need at least a standard 20-byte IPv4 header.
     */
    if (skb->len < IP_HEADER_SIZE) {
        serial_print("[ip] Packet too short\n");
        skb_free(skb);
        return -1;
    }

    ip_hdr = (struct ip_header *)skb->data;

    if (ip_parse_header(ip_hdr,
                        &src,
                        &dst,
                        &protocol,
                        &total_length) != 0) {
        serial_print("[ip] Invalid IP packet\n");
        skb_free(skb);
        return -1;
    }

    /*
     * Do not allow the IPv4 header to claim bytes that are not present
     * in the skb.
     */
    if (total_length > skb->len) {
        serial_print("[ip] IP packet truncated\n");
        skb_free(skb);
        return -1;
    }

    /*
     * Remove the IPv4 header.
     *
     * The resulting skb contains the transport-layer payload.
     */
    if (skb_pull(skb, IP_HEADER_SIZE) != 0) {
        skb_free(skb);
        return -1;
    }

    skb->src_ip = src;
    skb->dst_ip = dst;
    skb->protocol = protocol;

    switch (protocol) {
    case IP_PROTO_ICMP:
        return icmp_receive(skb);

    case IP_PROTO_UDP:
        return udp_receive(skb);

    case IP_PROTO_TCP:
        return tcp_receive(skb);

    default:
        serial_print("[ip] Unknown protocol: ");
        serial_print_dec(protocol);
        serial_print("\n");

        skb_free(skb);
        return -1;
    }
}

/* ============================================================================
 * IP Forward
 * ============================================================================ */

int ip_forward(struct sk_buff *skb)
{
    struct ip_header *ip_hdr;

    if (!skb)
        return -1;

    if (!skb->data)
        return -1;

    if (skb->len < IP_HEADER_SIZE)
        return -1;

    ip_hdr = (struct ip_header *)skb->data;

    /*
     * A packet with TTL <= 1 cannot be forwarded.
     */
    if (ip_hdr->ttl <= 1U) {
        serial_print("[ip] TTL expired\n");
        skb_free(skb);
        return -1;
    }

    ip_hdr->ttl--;

    /*
     * Recalculate IPv4 header checksum.
     */
    ip_hdr->checksum = 0;
    ip_hdr->checksum = ip_checksum(ip_hdr, IP_HEADER_SIZE);

    serial_print("[ip] Forwarding packet\n");

    return 0;
}

/* ============================================================================
 * Routing
 * ============================================================================ */

int ip_route(ip_addr_t *dest, ip_addr_t *next_hop)
{
    uint32_t dest_ip;

    if (!dest || !next_hop)
        return -1;

    dest_ip = ip_addr_to_u32(dest);

    for (uint32_t i = 0; i < route_count; i++) {
        if (!route_table[i].valid)
            continue;

        if ((dest_ip & route_table[i].netmask) ==
            (route_table[i].dest & route_table[i].netmask)) {

            if (route_table[i].gateway == 0) {
                *next_hop = *dest;
            } else {
                ip_addr_from_u32(next_hop,
                                 route_table[i].gateway);
            }

            return 0;
        }
    }

    return -1;
}

int ip_add_route(uint32_t dest,
                 uint32_t netmask,
                 uint32_t gateway,
                 uint32_t dev_id)
{
    if (route_count >= ROUTE_TABLE_SIZE)
        return -1;

    route_table[route_count].dest = dest;
    route_table[route_count].netmask = netmask;
    route_table[route_count].gateway = gateway;
    route_table[route_count].dev_id = dev_id;
    route_table[route_count].valid = 1;

    route_count++;

    return 0;
}

/* ============================================================================
 * IP Input
 * ============================================================================ */

int ip_input(struct sk_buff *skb)
{
    return ip_receive(skb);
}

/* ============================================================================
 * IP Output
 * ============================================================================ */

int ip_output(struct sk_buff *skb,
              ip_addr_t *src,
              ip_addr_t *dst,
              uint8_t protocol)
{
    return ip_send(skb, src, dst, protocol);
}

/* ============================================================================
 * IP Initialization
 * ============================================================================ */

int ip_init(void)
{
    serial_print("[ip] Initializing...\n");

    if (ip_initialized) {
        serial_print("[ip] Already initialized\n");
        return 0;
    }

    /*
     * Reset routing table.
     */
    memset(route_table, 0, sizeof(route_table));
    route_count = 0;

    /*
     * Default local IP.
     */
    ip_local_addr.addr[0] = 192;
    ip_local_addr.addr[1] = 168;
    ip_local_addr.addr[2] = 1;
    ip_local_addr.addr[3] = 100;

    /*
     * Default route.
     */
    if (ip_add_route(0, 0, 0, 0) != 0) {
        serial_print("[ip] Failed to add default route\n");
        return -1;
    }

    ip_initialized = 1;

    serial_print("[ip] Initialized\n");

    return 0;
}

/* ============================================================================
 * IP Shutdown
 * ============================================================================ */

int ip_shutdown(void)
{
    serial_print("[ip] Shutting down...\n");

    ip_initialized = 0;

    serial_print("[ip] Shutdown complete\n");

    return 0;
}
