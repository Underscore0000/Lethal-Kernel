/*
 * Lethal Kernel - ARP Header
 *
 * v5.0: Address Resolution Protocol header.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_IPV4_ARP_H
#define LETHAL_NET_IPV4_ARP_H

#include <stdint.h>
#include <stddef.h>
#include "../core/net.h"
#include "../core/sk_buff.h"

/* Function prototypes */
int arp_init(void);
int arp_shutdown(void);

int arp_input(struct sk_buff *skb);
int arp_receive(struct sk_buff *skb);
int arp_resolve(uint32_t ip, uint8_t *mac);

int arp_cache_add(uint32_t ip, uint8_t *mac);
int arp_cache_lookup(uint32_t ip, uint8_t *mac);
void arp_cache_clear(void);

int arp_build_packet(uint8_t *buffer, uint32_t sender_ip, uint8_t *sender_mac,
                      uint32_t target_ip, uint8_t *target_mac, uint16_t op);

#endif /* LETHAL_NET_IPV4_ARP_H */