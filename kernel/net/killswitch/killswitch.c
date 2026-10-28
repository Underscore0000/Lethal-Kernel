/*
 * Lethal Kernel - Kill Switch Implementation
 *
 * v5.0: Real kill switch to block all non-VPN/Tor traffic.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "killswitch.h"
#include "../core/sk_buff.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include "../config/privacy.h"
#include <string.h>
#include <stdio.h>

static struct killswitch_status ks_status;
static uint8_t ks_initialized = 0;

#define EXCEPTION_MAX 64

struct exception_entry {
    uint32_t ip;
    uint16_t port;
    uint8_t valid;
};

static struct exception_entry exceptions[EXCEPTION_MAX];
static uint32_t exception_count = 0;

int killswitch_init(void)
{
    serial_print("[killswitch] Initializing...\n");
    
    if (ks_initialized) {
        serial_print("[killswitch] Already initialized\n");
        return 0;
    }
    
    memset(&ks_status, 0, sizeof(ks_status));
    ks_status.state = KILLSWITCH_STATE_OFF;
    ks_status.mode = KILLSWITCH_MODE_STRICT;
    ks_status.vpn_active = 0;
    ks_status.tor_active = 0;
    
    memset(exceptions, 0, sizeof(exceptions));
    exception_count = 0;
    
    ks_initialized = 1;
    
    serial_print("[killswitch] Initialized (disabled by default)\n");
    return 0;
}

int killswitch_enable(uint8_t mode)
{
    if (!ks_initialized) return -1;
    
    if (mode != KILLSWITCH_MODE_STRICT && mode != KILLSWITCH_MODE_PERMISSIVE) {
        return -1;
    }
    
    ks_status.mode = mode;
    ks_status.state = KILLSWITCH_STATE_ON;
    ks_status.blocked_packets = 0;
    ks_status.allowed_packets = 0;
    
    serial_print("[killswitch] Enabled\n");
    return 0;
}

int killswitch_disable(void)
{
    if (!ks_initialized) return -1;
    
    ks_status.state = KILLSWITCH_STATE_OFF;
    strcpy(ks_status.last_event, "Disabled by user");
    
    serial_print("[killswitch] Disabled\n");
    return 0;
}

int killswitch_trigger(uint32_t reason)
{
    if (!ks_initialized) return -1;
    
    if (ks_status.state == KILLSWITCH_STATE_OFF) {
        serial_print("[killswitch] Kill switch is disabled, ignoring trigger\n");
        return 0;
    }
    
    ks_status.state = KILLSWITCH_STATE_TRIGGERED;
    ks_status.triggered_by = reason;
    
    const char *reason_str = "";
    switch(reason) {
        case KILLSWITCH_TRIGGER_VPN_DOWN:   reason_str = "VPN connection lost"; break;
        case KILLSWITCH_TRIGGER_TOR_DOWN:   reason_str = "Tor connection lost"; break;
        case KILLSWITCH_TRIGGER_MANUAL:     reason_str = "Manual trigger"; break;
        case KILLSWITCH_TRIGGER_LEAK_DETECT: reason_str = "DNS leak detected"; break;
        default:                            reason_str = "Unknown"; break;
    }
    
    snprintf(ks_status.last_event, sizeof(ks_status.last_event),
             "Triggered: %s", reason_str);
    
    serial_print("[killswitch] TRIGGERED: ");
    serial_print(reason_str);
    serial_print("\n");
    
    return 0;
}

int killswitch_reset(void)
{
    if (!ks_initialized) return -1;
    
    ks_status.state = KILLSWITCH_STATE_ON;
    ks_status.blocked_packets = 0;
    ks_status.allowed_packets = 0;
    ks_status.triggered_by = 0;
    strcpy(ks_status.last_event, "Reset");
    
    serial_print("[killswitch] Reset\n");
    return 0;
}

int killswitch_get_status(struct killswitch_status *status)
{
    if (!status || !ks_initialized) return -1;
    memcpy(status, &ks_status, sizeof(struct killswitch_status));
    return 0;
}

int killswitch_is_enabled(void)
{
    if (!ks_initialized) return 0;
    return (ks_status.state == KILLSWITCH_STATE_ON ||
            ks_status.state == KILLSWITCH_STATE_TRIGGERED);
}

int killswitch_is_blocked(void)
{
    if (!ks_initialized) return 0;
    return (ks_status.state == KILLSWITCH_STATE_TRIGGERED);
}

uint32_t killswitch_get_blocked_count(void)
{
    if (!ks_initialized) return 0;
    return ks_status.blocked_packets;
}

int killswitch_vpn_up(void)
{
    if (!ks_initialized) return -1;
    
    ks_status.vpn_active = 1;
    strcpy(ks_status.last_event, "VPN up");
    
    serial_print("[killswitch] VPN is up\n");
    
    if (ks_status.state == KILLSWITCH_STATE_TRIGGERED &&
        ks_status.triggered_by == KILLSWITCH_TRIGGER_VPN_DOWN) {
        killswitch_reset();
    }
    
    return 0;
}

int killswitch_vpn_down(void)
{
    if (!ks_initialized) return -1;
    
    ks_status.vpn_active = 0;
    
    serial_print("[killswitch] VPN is DOWN\n");
    
    if (ks_status.state == KILLSWITCH_STATE_ON ||
        ks_status.state == KILLSWITCH_STATE_TRIGGERED) {
        killswitch_trigger(KILLSWITCH_TRIGGER_VPN_DOWN);
    }
    
    return 0;
}

int killswitch_tor_up(void)
{
    if (!ks_initialized) return -1;
    
    ks_status.tor_active = 1;
    strcpy(ks_status.last_event, "Tor up");
    
    serial_print("[killswitch] Tor is up\n");
    
    if (ks_status.state == KILLSWITCH_STATE_TRIGGERED &&
        ks_status.triggered_by == KILLSWITCH_TRIGGER_TOR_DOWN) {
        killswitch_reset();
    }
    
    return 0;
}

int killswitch_tor_down(void)
{
    if (!ks_initialized) return -1;
    
    ks_status.tor_active = 0;
    
    serial_print("[killswitch] Tor is DOWN\n");
    
    if (ks_status.state == KILLSWITCH_STATE_ON ||
        ks_status.state == KILLSWITCH_STATE_TRIGGERED) {
        killswitch_trigger(KILLSWITCH_TRIGGER_TOR_DOWN);
    }
    
    return 0;
}

int killswitch_should_block_packet(struct sk_buff *skb)
{
    if (!ks_initialized || !skb) return 0;
    
    if (ks_status.state == KILLSWITCH_STATE_OFF) {
        return 0;
    }
    
    if (ks_status.state == KILLSWITCH_STATE_TRIGGERED) {
        ks_status.blocked_packets++;
        return 1;
    }
    
    if (ks_status.mode == KILLSWITCH_MODE_STRICT) {
        if (!ks_status.vpn_active && !ks_status.tor_active) {
            ks_status.blocked_packets++;
            return 1;
        }
    }
    
    ks_status.allowed_packets++;
    return 0;
}

int killswitch_should_block_connection(uint32_t dest_ip, uint16_t dest_port)
{
    if (!ks_initialized) return 0;
    
    if (ks_status.state == KILLSWITCH_STATE_OFF) {
        return 0;
    }
    
    if (ks_status.state == KILLSWITCH_STATE_TRIGGERED) {
        return 1;
    }
    
    if (ks_status.mode == KILLSWITCH_MODE_STRICT) {
        if (!ks_status.vpn_active && !ks_status.tor_active) {
            return 1;
        }
    }
    
    for (uint32_t i = 0; i < exception_count; i++) {
        if (!exceptions[i].valid) continue;
        if (exceptions[i].ip == dest_ip &&
            exceptions[i].port == dest_port) {
            return 0;
        }
    }
    
    return 0;
}

int killswitch_should_block_resolve(const char *hostname)
{
    if (!ks_initialized || !hostname) return 0;
    
    if (ks_status.state == KILLSWITCH_STATE_TRIGGERED) {
        return 1;
    }
    
    if (ks_status.mode == KILLSWITCH_MODE_STRICT) {
        if (!ks_status.vpn_active && !ks_status.tor_active) {
            return 1;
        }
    }
    
    return 0;
}

int killswitch_set_mode(uint8_t mode)
{
    if (!ks_initialized) return -1;
    
    if (mode != KILLSWITCH_MODE_STRICT && mode != KILLSWITCH_MODE_PERMISSIVE) {
        return -1;
    }
    
    ks_status.mode = mode;
    
    serial_print("[killswitch] Mode set\n");
    return 0;
}

int killswitch_add_exception(uint32_t ip, uint16_t port)
{
    if (!ks_initialized) return -1;
    if (exception_count >= EXCEPTION_MAX) return -1;
    
    exceptions[exception_count].ip = ip;
    exceptions[exception_count].port = port;
    exceptions[exception_count].valid = 1;
    exception_count++;
    
    serial_print("[killswitch] Added exception\n");
    return 0;
}

int killswitch_remove_exception(uint32_t ip, uint16_t port)
{
    if (!ks_initialized) return -1;
    
    for (uint32_t i = 0; i < exception_count; i++) {
        if (exceptions[i].valid &&
            exceptions[i].ip == ip &&
            exceptions[i].port == port) {
            exceptions[i].valid = 0;
            return 0;
        }
    }
    
    return -1;
}