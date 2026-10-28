/*
 * Lethal Kernel - UDP Implementation
 *
 * v5.0: UDP protocol with random source ports.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */

#include "udp.h"
#include "ip.h"
#include "../core/device.h"
#include "../config/privacy.h"
#include "../../core/console.h"
#include "../../memory/heap.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#define UDP_SOCKET_MAX 64

static struct udp_socket udp_sockets[UDP_SOCKET_MAX];
static uint16_t udp_next_port = 1024;

/* ============================================================================
 * Byte Order Helpers
 * ============================================================================ */

static uint16_t udp_htons(uint16_t x)
{
    return (uint16_t)(((x & 0x00FFU) << 8) |
                      ((x & 0xFF00U) >> 8));
}

static uint16_t udp_ntohs(uint16_t x)
{
    return udp_htons(x);
}

/* ============================================================================
 * UDP Checksum
 * ============================================================================ */

static uint16_t udp_checksum(struct udp_header *udp,
                             uint8_t *data,
                             size_t len,
                             uint32_t src_ip,
                             uint32_t dst_ip)
{
    uint32_t sum = 0;
    uint8_t *bytes = (uint8_t *)udp;
    size_t i;

    /*
     * IPv4 pseudo-header.
     *
     * src_ip/dst_ip are stored in network-order representation by the
     * existing IP layer, so the 16-bit words are consumed directly.
     */
    sum += (src_ip >> 16) & 0xFFFFU;
    sum += src_ip & 0xFFFFU;

    sum += (dst_ip >> 16) & 0xFFFFU;
    sum += dst_ip & 0xFFFFU;

    sum += udp_htons(IP_PROTO_UDP);
    sum += udp_htons((uint16_t)(len + sizeof(struct udp_header)));

    /*
     * UDP header.
     *
     * The checksum field must already be zero when this function is called.
     */
    for (i = 0; i < sizeof(struct udp_header); i += 2) {
        uint16_t word;

        word = (uint16_t)((uint16_t)bytes[i] << 8);

        if (i + 1 < sizeof(struct udp_header)) {
            word |= bytes[i + 1];
        }

        sum += word;
    }

    /*
     * UDP payload.
     */
    for (i = 0; i < len; i += 2) {
        uint16_t word;

        word = (uint16_t)((uint16_t)data[i] << 8);

        if (i + 1 < len) {
            word |= data[i + 1];
        }

        sum += word;
    }

    /*
     * End-around carry.
     */
    while (sum >> 16) {
        sum = (sum & 0xFFFFU) + (sum >> 16);
    }

    return (uint16_t)~sum;
}

/* ============================================================================
 * UDP Send
 * ============================================================================ */

int udp_send(struct sk_buff *skb,
             ip_addr_t *src,
             uint16_t src_port,
             ip_addr_t *dst,
             uint16_t dst_port)
{
    struct udp_header *udp;
    size_t payload_len;
    uint16_t checksum;

    if (!skb || !src || !dst)
        return -1;

    /*
     * Add UDP header in front of the payload.
     */
    if (skb_push(skb, sizeof(struct udp_header)) != 0)
        return -1;

    udp = (struct udp_header *)skb->data;

    payload_len = skb->len - sizeof(struct udp_header);

    udp->src_port = udp_htons(src_port);
    udp->dst_port = udp_htons(dst_port);
    udp->length = udp_htons(
        (uint16_t)(sizeof(struct udp_header) + payload_len)
    );
    udp->checksum = 0;

    checksum = udp_checksum(
        udp,
        skb->data + sizeof(struct udp_header),
        payload_len,
        ip_addr_to_u32(src),
        ip_addr_to_u32(dst)
    );

    /*
     * IPv4 permits a zero UDP checksum, but generating a real checksum is
     * preferable.
     */
    udp->checksum = checksum;

    return ip_send(skb, src, dst, IP_PROTO_UDP);
}

/* ============================================================================
 * UDP Receive
 * ============================================================================ */

int udp_receive(struct sk_buff *skb)
{
    struct udp_header *udp;
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t udp_len;

    if (!skb)
        return -1;

    if (skb->len < sizeof(struct udp_header)) {
        serial_print("[udp] Packet too small\n");
        skb_free(skb);
        return -1;
    }

    udp = (struct udp_header *)skb->data;

    src_port = udp_ntohs(udp->src_port);
    dst_port = udp_ntohs(udp->dst_port);
    udp_len = udp_ntohs(udp->length);

    /*
     * UDP length includes the UDP header itself.
     */
    if (udp_len < sizeof(struct udp_header) ||
        udp_len > skb->len) {
        serial_print("[udp] Invalid UDP length\n");
        skb_free(skb);
        return -1;
    }

    /*
     * Remove UDP header and leave the payload in skb->data.
     */
    if (skb_pull(skb, sizeof(struct udp_header)) != 0) {
        skb_free(skb);
        return -1;
    }

    serial_print("[udp] Received from port ");
    serial_print_dec(src_port);
    serial_print(" to port ");
    serial_print_dec(dst_port);
    serial_print("\n");

    /*
     * TODO:
     *
     * Deliver payload to the matching UDP socket.
     */

    skb_free(skb);

    return 0;
}

/* ============================================================================
 * UDP Socket Management
 * ============================================================================ */

int udp_socket_create(void)
{
    int i;

    for (i = 0; i < UDP_SOCKET_MAX; i++) {
        if (!udp_sockets[i].in_use) {
            memset(&udp_sockets[i],
                   0,
                   sizeof(struct udp_socket));

            udp_sockets[i].fd = (uint32_t)(i + 300);
            udp_sockets[i].in_use = 1;

            return (int)udp_sockets[i].fd;
        }
    }

    return -1;
}

/* ============================================================================
 * UDP Socket Bind
 * ============================================================================ */

int udp_socket_bind(int fd, uint16_t port)
{
    int i;

    for (i = 0; i < UDP_SOCKET_MAX; i++) {
        if (!udp_sockets[i].in_use)
            continue;

        if ((int)udp_sockets[i].fd != fd)
            continue;

        if (port == 0) {
            /*
             * Automatically choose a source port.
             */
            udp_sockets[i].src_port =
                (uint16_t)(rand() % 60000 + 1024);
        } else {
            udp_sockets[i].src_port = port;
        }

        udp_sockets[i].bound = 1;

        return 0;
    }

    return -1;
}

/* ============================================================================
 * UDP Socket SendTo
 * ============================================================================ */

int udp_socket_sendto(int fd,
                      const void *data,
                      size_t len,
                      ip_addr_t *dst,
                      uint16_t port)
{
    struct udp_socket *sock;
    struct sk_buff *skb;
    ip_addr_t src;
    int socket_idx;
    int bind_result;

    if (!data || !dst || len == 0)
        return -1;

    socket_idx = -1;

    /*
     * Find socket by file descriptor.
     */
    for (int i = 0; i < UDP_SOCKET_MAX; i++) {
        if (udp_sockets[i].in_use &&
            (int)udp_sockets[i].fd == fd) {

            socket_idx = i;
            break;
        }
    }

    if (socket_idx < 0)
        return -1;

    sock = &udp_sockets[socket_idx];

    /*
     * Allocate an ephemeral source port when the socket has not yet
     * been bound.
     */
    if (!sock->bound) {
        bind_result = udp_socket_bind(fd, 0);

        if (bind_result != 0)
            return -1;
    }

    /*
     * Get the local IPv4 address from the IP layer.
     *
     * Do NOT use dev->ip here: struct net_device does not expose an
     * IPv4 address field in this kernel.
     */
    memset(&src, 0, sizeof(src));
    ip_get_local(&src);

    /*
     * Save the socket addresses for bookkeeping.
     */
    sock->src_ip = src;
    sock->dst_ip = *dst;
    sock->dst_port = port;

    /*
     * Allocate enough headroom for:
     *
     *   Ethernet header
     *   IPv4 header
     *   UDP header
     *   payload
     */
    skb = skb_alloc(len + 128);

    if (!skb)
        return -1;

    /*
     * Append payload.
     */
    if (skb_put(skb, len) != 0) {
        skb_free(skb);
        return -1;
    }

    memcpy(skb->data + skb->len - len, data, len);

    /*
     * Build UDP header and pass packet to IPv4.
     */
    return udp_send(
        skb,
        &src,
        sock->src_port,
        dst,
        port
    );
}

/* ============================================================================
 * UDP Initialization
 * ============================================================================ */

int udp_init(void)
{
    serial_print("[udp] Initializing...\n");

    memset(udp_sockets, 0, sizeof(udp_sockets));

    udp_next_port = 1024;

    serial_print("[udp] Initialized\n");

    return 0;
}
