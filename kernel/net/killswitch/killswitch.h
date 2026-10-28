/*
 * Lethal Kernel - Kill Switch
 *
 * v5.0: Network kill switch to block all traffic if VPN/Tor fails.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_KILLSWITCH_H
#define LETHAL_NET_KILLSWITCH_H

#include <stdint.h>
#include <stddef.h>
#include "../core/sk_buff.h"

#define KILLSWITCH_STATE_OFF       0
#define KILLSWITCH_STATE_ON        1
#define KILLSWITCH_STATE_TRIGGERED 2

#define KILLSWITCH_MODE_STRICT     0
#define KILLSWITCH_MODE_PERMISSIVE 1

struct killswitch_status {
    uint8_t state;
    uint8_t mode;
    uint32_t blocked_packets;
    uint32_t allowed_packets;
    uint8_t vpn_active;
    uint8_t tor_active;
    uint8_t triggered_by;
    char last_event[64];
};

#define KILLSWITCH_TRIGGER_VPN_DOWN   1
#define KILLSWITCH_TRIGGER_TOR_DOWN   2
#define KILLSWITCH_TRIGGER_MANUAL     3
#define KILLSWITCH_TRIGGER_LEAK_DETECT 4

int killswitch_init(void);
int killswitch_enable(uint8_t mode);
int killswitch_disable(void);
int killswitch_trigger(uint32_t reason);
int killswitch_reset(void);
int killswitch_get_status(struct killswitch_status *status);
int killswitch_is_enabled(void);
int killswitch_is_blocked(void);
uint32_t killswitch_get_blocked_count(void);

int killswitch_vpn_up(void);
int killswitch_vpn_down(void);
int killswitch_tor_up(void);
int killswitch_tor_down(void);

int killswitch_should_block_packet(struct sk_buff *skb);
int killswitch_should_block_connection(uint32_t dest_ip, uint16_t dest_port);
int killswitch_should_block_resolve(const char *hostname);

int killswitch_set_mode(uint8_t mode);
int killswitch_add_exception(uint32_t ip, uint16_t port);
int killswitch_remove_exception(uint32_t ip, uint16_t port);

#endif