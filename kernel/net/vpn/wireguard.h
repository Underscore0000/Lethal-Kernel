/*
 * Lethal Kernel - WireGuard Header
 *
 * v5.0: WireGuard VPN protocol implementation.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_VPN_WIREGUARD_H
#define LETHAL_NET_VPN_WIREGUARD_H

#include <stdint.h>
#include "../core/sk_buff.h"
#include "vpn.h"

/* WireGuard message types */
#define WG_MSG_HANDSHAKE_INIT   1
#define WG_MSG_HANDSHAKE_RESP   2
#define WG_MSG_DATA             3
#define WG_MSG_COOKIE_REPLY     4

/* WireGuard header */
struct wireguard_header {
    uint8_t type;
    uint8_t reserved[3];
    uint32_t key_index;
    uint64_t counter;
    uint8_t encrypted_data[0];
} __attribute__((packed));

/* ============================================================================
 * Function Prototypes
 * ============================================================================ */

int vpn_wireguard_init(void);
int vpn_wireguard_handshake(void);
int vpn_wireguard_encrypt(struct sk_buff *skb);
int vpn_wireguard_decrypt(struct sk_buff *skb);
int vpn_wireguard_send_handshake(void);
int vpn_wireguard_recv_handshake(struct sk_buff *skb);

#endif /* LETHAL_NET_VPN_WIREGUARD_H */