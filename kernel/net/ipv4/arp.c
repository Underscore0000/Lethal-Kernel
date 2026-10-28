/*
 * Lethal Kernel - ARP Implementation
 *
 * v5.0: Address Resolution Protocol.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "arp.h"
#include "../core/device.h"
#include "../core/net.h"
#include "../core/sk_buff.h"
#include "../core/ethernet.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>

#define ARP_CACHE_SIZE 16

/* Byte-order functions */
static inline uint16_t htons(uint16_t x)
{
    return ((x & 0xFF) << 8) | ((x & 0xFF00) >> 8);
}

static inline uint16_t ntohs(uint16_t x)
{
    return ((x & 0xFF) << 8) | ((x & 0xFF00) >> 8);
}

struct arp_entry {
    uint32_t ip;
    uint8_t mac[6];
    uint8_t valid;
};

static struct arp_entry arp_cache[ARP_CACHE_SIZE];
static uint8_t arp_initialized = 0;

/* ============================================================================
 * ARP Header
 * ============================================================================ */

struct arp_header {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_len;
    uint8_t proto_len;
    uint16_t op;
    uint8_t sender_mac[6];
    uint32_t sender_ip;
    uint8_t target_mac[6];
    uint32_t target_ip;
} __attribute__((packed));

/* ============================================================================
 * Initialization
 * ============================================================================ */

int arp_init(void)
{
    serial_print("[arp] Initializing...\n");
    
    if (arp_initialized) {
        serial_print("[arp] Already initialized\n");
        return 0;
    }
    
    memset(arp_cache, 0, sizeof(arp_cache));
    arp_initialized = 1;
    
    serial_print("[arp] Initialized\n");
    return 0;
}

int arp_shutdown(void)
{
    serial_print("[arp] Shutting down...\n");
    arp_initialized = 0;
    serial_print("[arp] Shutdown complete\n");
    return 0;
}

/* ============================================================================
 * ARP Cache
 * ============================================================================ */

int arp_cache_add(uint32_t ip, uint8_t *mac)
{
    if (!mac) return -1;
    
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            memcpy(arp_cache[i].mac, mac, 6);
            return 0;
        }
    }
    
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) {
            arp_cache[i].ip = ip;
            memcpy(arp_cache[i].mac, mac, 6);
            arp_cache[i].valid = 1;
            return 0;
        }
    }
    
    return -1;
}

int arp_cache_lookup(uint32_t ip, uint8_t *mac)
{
    if (!mac) return -1;
    
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            memcpy(mac, arp_cache[i].mac, 6);
            return 0;
        }
    }
    return -1;
}

void arp_cache_clear(void)
{
    memset(arp_cache, 0, sizeof(arp_cache));
}

/* ============================================================================
 * ARP Send Request
 * ============================================================================ */

static int arp_send_request(struct net_device *dev, uint32_t target_ip)
{
    if (!dev) return -1;
    
    struct sk_buff *skb = skb_alloc(ETH_HEADER_SIZE + sizeof(struct arp_header));
    if (!skb) return -1;
    
    /* Build Ethernet header */
    struct eth_header *eth = (struct eth_header *)skb->data;
    memset(eth->dst_mac, 0xFF, 6);
    memcpy(eth->src_mac, dev->mac, 6);
    eth->ethertype = htons(ETH_P_ARP);
    
    /* Build ARP header */
    struct arp_header *arp = (struct arp_header *)(skb->data + ETH_HEADER_SIZE);
    arp->hw_type = htons(1); /* Ethernet */
    arp->proto_type = htons(ETH_P_IP);
    arp->hw_len = 6;
    arp->proto_len = 4;
    arp->op = htons(1); /* Request */
    memcpy(arp->sender_mac, dev->mac, 6);
    arp->sender_ip = 0; /* Will be filled by IP layer */
    memset(arp->target_mac, 0, 6);
    arp->target_ip = target_ip;
    
    skb->len = ETH_HEADER_SIZE + sizeof(struct arp_header);
    
    serial_print("[arp] Sending ARP request for IP ");
    serial_print_dec((target_ip >> 24) & 0xFF);
    serial_print(".");
    serial_print_dec((target_ip >> 16) & 0xFF);
    serial_print(".");
    serial_print_dec((target_ip >> 8) & 0xFF);
    serial_print(".");
    serial_print_dec(target_ip & 0xFF);
    serial_print("\n");
    
    return net_device_transmit(dev, skb);
}

/* ============================================================================
 * ARP Reply
 * ============================================================================ */

static int arp_reply(struct net_device *dev, struct arp_header *req)
{
    if (!dev || !req) return -1;
    
    struct sk_buff *skb = skb_alloc(ETH_HEADER_SIZE + sizeof(struct arp_header));
    if (!skb) return -1;
    
    /* Build Ethernet header */
    struct eth_header *eth = (struct eth_header *)skb->data;
    memcpy(eth->dst_mac, req->sender_mac, 6);
    memcpy(eth->src_mac, dev->mac, 6);
    eth->ethertype = htons(ETH_P_ARP);
    
    /* Build ARP header */
    struct arp_header *arp = (struct arp_header *)(skb->data + ETH_HEADER_SIZE);
    arp->hw_type = htons(1);
    arp->proto_type = htons(ETH_P_IP);
    arp->hw_len = 6;
    arp->proto_len = 4;
    arp->op = htons(2); /* Reply */
    memcpy(arp->sender_mac, dev->mac, 6);
    arp->sender_ip = req->target_ip;
    memcpy(arp->target_mac, req->sender_mac, 6);
    arp->target_ip = req->sender_ip;
    
    skb->len = ETH_HEADER_SIZE + sizeof(struct arp_header);
    
    serial_print("[arp] Sending ARP reply\n");
    
    return net_device_transmit(dev, skb);
}

/* ============================================================================
 * ARP Receive
 * ============================================================================ */

int arp_receive(struct sk_buff *skb)
{
    if (!skb) return -1;
    
    struct arp_header *arp = (struct arp_header *)skb->data;
    
    if (arp->hw_type != htons(1) || arp->proto_type != htons(ETH_P_IP)) {
        skb_free(skb);
        return -1;
    }
    
    uint32_t sender_ip = arp->sender_ip;
    uint16_t op = ntohs(arp->op);
    
    /* Add to cache */
    arp_cache_add(sender_ip, arp->sender_mac);
    
    if (op == 1) { /* ARP Request */
        /* Check if target IP is ours */
        struct net_device *dev = net_device_get(0);
        if (dev) {
            /* For now, just reply to all requests */
            arp_reply(dev, arp);
        }
    }
    
    skb_free(skb);
    return 0;
}

/* ============================================================================
 * ARP Resolve
 * ============================================================================ */

int arp_resolve(uint32_t ip, uint8_t *mac)
{
    if (!mac) return -1;
    
    /* Check cache first */
    if (arp_cache_lookup(ip, mac) == 0) {
        return 0;
    }
    
    /* Send ARP request */
    struct net_device *dev = net_device_get(0);
    if (!dev) return -1;
    
    arp_send_request(dev, ip);
    
    return -1; /* Not resolved yet */
}

/* ============================================================================
 * ARP Input (wrapper)
 * ============================================================================ */

int arp_input(struct sk_buff *skb)
{
    return arp_receive(skb);
}

/* ============================================================================
 * ARP Packet Building
 * ============================================================================ */

int arp_build_packet(uint8_t *buffer, uint32_t sender_ip, uint8_t *sender_mac,
                      uint32_t target_ip, uint8_t *target_mac, uint16_t op)
{
    if (!buffer) return -1;
    
    struct arp_header *arp = (struct arp_header *)buffer;
    arp->hw_type = htons(1);
    arp->proto_type = htons(ETH_P_IP);
    arp->hw_len = 6;
    arp->proto_len = 4;
    arp->op = htons(op);
    
    if (sender_mac) {
        memcpy(arp->sender_mac, sender_mac, 6);
    } else {
        memset(arp->sender_mac, 0, 6);
    }
    arp->sender_ip = sender_ip;
    
    if (target_mac) {
        memcpy(arp->target_mac, target_mac, 6);
    } else {
        memset(arp->target_mac, 0, 6);
    }
    arp->target_ip = target_ip;
    
    return sizeof(struct arp_header);
}