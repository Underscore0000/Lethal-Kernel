/*
 * Lethal Kernel - ICMP Implementation
 *
 * v5.0: Internet Control Message Protocol.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */

#include "icmp.h"
#include "ip.h"
#include "../core/device.h"
#include "../core/net.h"
#include "../core/sk_buff.h"
#include "../../core/console.h"
#include "../../memory/heap.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

static uint8_t icmp_initialized = 0;

/* ============================================================================
 * ICMP Constants
 * ============================================================================ */

#define ICMP_HEADER_SIZE        8U
#define ICMP_ECHO_PAYLOAD_SIZE  32U
#define ICMP_PACKET_SIZE        (ICMP_HEADER_SIZE + ICMP_ECHO_PAYLOAD_SIZE)

/* ============================================================================
 * Byte Order Helpers
 * ============================================================================ */

/*
 * Convert a 16-bit value between host and network byte order.
 *
 * x86-64 is little-endian, so this swaps the two bytes.
 */
static uint16_t icmp_htons(uint16_t value)
{
    return (uint16_t)((value << 8) | (value >> 8));
}

static uint16_t icmp_ntohs(uint16_t value)
{
    return icmp_htons(value);
}

/* ============================================================================
 * ICMP Checksum
 * ============================================================================ */

/*
 * Internet checksum.
 *
 * The packet buffer may not be aligned to a uint16_t boundary, therefore
 * we deliberately read the packet byte-by-byte instead of casting it to
 * uint16_t *.
 */
static uint16_t icmp_checksum(const void *data, size_t len)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t sum = 0;

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
 * ICMP Packet Validation
 * ============================================================================ */

static int icmp_validate_packet(struct sk_buff *skb)
{
    if (!skb)
        return -1;

    if (!skb->data)
        return -1;

    if (skb->len < ICMP_HEADER_SIZE) {
        serial_print("[icmp] Packet too short\n");
        return -1;
    }

    return 0;
}

/* ============================================================================
 * ICMP Send Echo Request
 * ============================================================================ */

int icmp_send_echo_request(ip_addr_t *dst, uint16_t id, uint16_t seq)
{
    struct net_device *dev;
    ip_addr_t src;
    struct sk_buff *skb;
    struct icmp_header *icmp;
    uint8_t *payload;

    if (!dst)
        return -1;

    dev = net_device_get(0);
    if (!dev)
        return -1;

    /*
     * The device lookup above verifies that a network device exists.
     */
    (void)dev;

    /*
     * ip_get_local() returns void in this kernel.
     */
    ip_get_local(&src);

    /*
     * Allocate enough room for:
     *
     *   8 bytes  ICMP header
     *   32 bytes echo payload
     */
    skb = skb_alloc(ICMP_PACKET_SIZE);
    if (!skb)
        return -1;

    icmp = (struct icmp_header *)skb->data;

    /*
     * ICMP Echo Request:
     *
     *   type     = 8
     *   code     = 0
     *   checksum = calculated below
     */
    icmp->type = ICMP_TYPE_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;

    icmp->data.echo.id = icmp_htons(id);
    icmp->data.echo.sequence = icmp_htons(seq);

    /*
     * Deterministic payload.
     */
    payload = skb->data + ICMP_HEADER_SIZE;

    for (size_t i = 0; i < ICMP_ECHO_PAYLOAD_SIZE; i++) {
        payload[i] = (uint8_t)('A' + (i % 26U));
    }

    skb->len = ICMP_PACKET_SIZE;

    /*
     * Calculate checksum after the entire ICMP packet has been populated.
     */
    icmp->checksum = icmp_checksum(skb->data, skb->len);

    serial_print("[icmp] Sending echo request to ");
    serial_print_dec(dst->addr[0]);
    serial_print(".");
    serial_print_dec(dst->addr[1]);
    serial_print(".");
    serial_print_dec(dst->addr[2]);
    serial_print(".");
    serial_print_dec(dst->addr[3]);
    serial_print("\n");

    return ip_send(skb, &src, dst, IP_PROTO_ICMP);
}

/* ============================================================================
 * ICMP Send Echo Reply
 * ============================================================================ */

int icmp_send_echo_reply(ip_addr_t *src,
                         ip_addr_t *dst,
                         uint16_t id,
                         uint16_t seq)
{
    struct net_device *dev;
    struct sk_buff *skb;
    struct icmp_header *icmp;
    uint8_t *payload;

    if (!src || !dst)
        return -1;

    dev = net_device_get(0);
    if (!dev)
        return -1;

    (void)dev;

    /*
     * Allocate a new packet for the reply.
     */
    skb = skb_alloc(ICMP_PACKET_SIZE);
    if (!skb)
        return -1;

    icmp = (struct icmp_header *)skb->data;

    /*
     * ICMP Echo Reply:
     *
     *   type     = 0
     *   code     = 0
     *   checksum = calculated below
     */
    icmp->type = ICMP_TYPE_ECHO_REPLY;
    icmp->code = 0;
    icmp->checksum = 0;

    icmp->data.echo.id = icmp_htons(id);
    icmp->data.echo.sequence = icmp_htons(seq);

    /*
     * Use the same payload format as locally generated echo requests.
     */
    payload = skb->data + ICMP_HEADER_SIZE;

    for (size_t i = 0; i < ICMP_ECHO_PAYLOAD_SIZE; i++) {
        payload[i] = (uint8_t)('A' + (i % 26U));
    }

    skb->len = ICMP_PACKET_SIZE;

    /*
     * Calculate the final ICMP checksum.
     */
    icmp->checksum = icmp_checksum(skb->data, skb->len);

    serial_print("[icmp] Sending echo reply\n");

    return ip_send(skb, src, dst, IP_PROTO_ICMP);
}

/* ============================================================================
 * ICMP Handle Echo Request
 * ============================================================================ */

int icmp_handle_echo_request(struct sk_buff *skb)
{
    struct icmp_header *icmp;
    uint16_t id;
    uint16_t seq;
    ip_addr_t local;

    if (icmp_validate_packet(skb) != 0)
        return -1;

    icmp = (struct icmp_header *)skb->data;

    /*
     * The ICMP header contains:
     *
     *   type      1 byte
     *   code      1 byte
     *   checksum  2 bytes
     *   echo      4 bytes
     *
     * so sizeof(struct icmp_header) must be at least 8 bytes.
     */
    if (skb->len < ICMP_HEADER_SIZE) {
        serial_print("[icmp] Echo request too short\n");
        return -1;
    }

    id = icmp_ntohs(icmp->data.echo.id);
    seq = icmp_ntohs(icmp->data.echo.sequence);

    /*
     * ip_get_local() returns void.
     */
    ip_get_local(&local);

    serial_print("[icmp] Received echo request\n");

    /*
     * The remote sender is skb->src_ip.
     *
     * Send:
     *
     *   local IP -> remote source IP
     */
    return icmp_send_echo_reply(&local,
                                &skb->src_ip,
                                id,
                                seq);
}

/* ============================================================================
 * ICMP Receive
 * ============================================================================ */

int icmp_receive(struct sk_buff *skb)
{
    struct icmp_header *icmp;
    uint16_t received_checksum;
    uint16_t calculated_checksum;
    int result = 0;

    if (icmp_validate_packet(skb) != 0) {
        if (skb)
            skb_free(skb);

        return -1;
    }

    icmp = (struct icmp_header *)skb->data;

    /*
     * Save the checksum received from the network.
     */
    received_checksum = icmp->checksum;

    /*
     * The checksum field must be zero while calculating the checksum.
     */
    icmp->checksum = 0;

    calculated_checksum = icmp_checksum(skb->data, skb->len);

    /*
     * Restore the packet before processing it.
     */
    icmp->checksum = received_checksum;

    if (calculated_checksum != received_checksum) {
        serial_print("[icmp] Invalid checksum\n");
        skb_free(skb);
        return -1;
    }

    switch (icmp->type) {
    case ICMP_TYPE_ECHO_REQUEST:
        /*
         * icmp_handle_echo_request() creates a new skb for the reply.
         * The received skb remains owned by this function.
         */
        result = icmp_handle_echo_request(skb);
        break;

    case ICMP_TYPE_ECHO_REPLY:
        serial_print("[icmp] Received echo reply\n");

        /*
         * TODO:
         *
         * Match identifier/sequence against outstanding echo requests.
         */
        break;

    case ICMP_TYPE_DEST_UNREACHABLE:
        serial_print("[icmp] Destination unreachable\n");
        break;

    case ICMP_TYPE_SOURCE_QUENCH:
        serial_print("[icmp] Source quench\n");
        break;

    case ICMP_TYPE_REDIRECT:
        serial_print("[icmp] Redirect\n");
        break;

    case ICMP_TYPE_TIME_EXCEEDED:
        serial_print("[icmp] Time exceeded\n");
        break;

    case ICMP_TYPE_PARAM_PROBLEM:
        serial_print("[icmp] Parameter problem\n");
        break;

    case ICMP_TYPE_TIMESTAMP_REQUEST:
        serial_print("[icmp] Timestamp request\n");
        break;

    case ICMP_TYPE_TIMESTAMP_REPLY:
        serial_print("[icmp] Timestamp reply\n");
        break;

    case ICMP_TYPE_INFO_REQUEST:
        serial_print("[icmp] Information request\n");
        break;

    case ICMP_TYPE_INFO_REPLY:
        serial_print("[icmp] Information reply\n");
        break;

    default:
        serial_print("[icmp] Unknown type: ");
        serial_print_dec(icmp->type);
        serial_print("\n");
        break;
    }

    /*
     * The receive path owns the incoming skb.
     */
    skb_free(skb);

    return result;
}

/* ============================================================================
 * ICMP Input
 * ============================================================================ */

int icmp_input(struct sk_buff *skb)
{
    return icmp_receive(skb);
}

/* ============================================================================
 * ICMP Initialization
 * ============================================================================ */

int icmp_init(void)
{
    serial_print("[icmp] Initializing...\n");

    if (icmp_initialized) {
        serial_print("[icmp] Already initialized\n");
        return 0;
    }

    icmp_initialized = 1;

    serial_print("[icmp] Initialized\n");

    return 0;
}

/* ============================================================================
 * ICMP Shutdown
 * ============================================================================ */

int icmp_shutdown(void)
{
    serial_print("[icmp] Shutting down...\n");

    icmp_initialized = 0;

    serial_print("[icmp] Shutdown complete\n");

    return 0;
}
