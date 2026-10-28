/*
 * Lethal Kernel - VPN Integration
 *
 * v5.0: Native VPN support with WireGuard integration.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_VPN_VPN_H
#define LETHAL_NET_VPN_VPN_H

#include <stdint.h>
#include <stddef.h>
#include "../core/net.h"
#include "../core/sk_buff.h"

/* VPN states */
#define VPN_STATE_OFF          0
#define VPN_STATE_INIT         1
#define VPN_STATE_CONNECTING   2
#define VPN_STATE_HANDSHAKE    3
#define VPN_STATE_ESTABLISHED  4
#define VPN_STATE_ERROR        5
#define VPN_STATE_RECONNECT    6

/* VPN protocols */
#define VPN_PROTO_WIREGUARD    1
#define VPN_PROTO_OPENVPN      2
#define VPN_PROTO_IPSEC        3
#define VPN_PROTO_L2TP         4
#define VPN_PROTO_SSTP         5

/* VPN configuration */
struct vpn_config {
    uint8_t enabled;
    uint8_t protocol;
    uint8_t auto_connect;
    uint8_t kill_switch_on;
    uint8_t dns_leak_protection;
    uint8_t ipv6_leak_protection;
    char server[256];
    uint16_t port;
    char username[64];
    char password[64];
    char private_key[128];
    char public_key[128];
    char allowed_ips[256];
    char endpoint[256];
    char dns_servers[256];
    uint32_t mtu;
    uint8_t persistent_keepalive;
    uint32_t timeout_ms;
};

/* VPN status */
struct vpn_status {
    uint8_t state;
    uint8_t protocol;
    ip_addr_t server_ip;
    ip_addr_t local_ip;
    ip_addr_t gateway_ip;
    uint32_t bytes_sent;
    uint32_t bytes_received;
    uint32_t handshake_time_ms;
    uint32_t uptime_sec;
    char server_name[64];
    char last_error[64];
};

/* VPN peer */
struct vpn_peer {
    ip_addr_t endpoint_ip;
    uint16_t endpoint_port;
    uint8_t public_key[32];
    uint8_t preshared_key[32];
    ip_addr_t allowed_ip;
    uint32_t allowed_cidr;
    uint8_t valid;
    void *next;
};

/* ============================================================================
 * Function Prototypes
 * ============================================================================ */

/* Initialization */
int vpn_init(void);
int vpn_shutdown(void);

/* Control functions */
int vpn_start(void);
int vpn_stop(void);
int vpn_restart(void);
int vpn_is_running(void);
int vpn_get_status(struct vpn_status *status);

/* Connection management */
int vpn_connect(const char *server, uint16_t port);
int vpn_disconnect(void);
int vpn_reconnect(void);
int vpn_wait_for_connection(uint32_t timeout_ms);

/* Configuration */
int vpn_set_config(struct vpn_config *config);
int vpn_get_config(struct vpn_config *config);
int vpn_set_protocol(uint8_t protocol);
int vpn_set_server(const char *server, uint16_t port);
int vpn_set_credentials(const char *username, const char *password);
int vpn_set_keys(const char *private_key, const char *public_key);
int vpn_set_allowed_ips(const char *cidrs);

/* Peer management */
int vpn_add_peer(struct vpn_peer *peer);
int vpn_remove_peer(ip_addr_t *allowed_ip);
int vpn_clear_peers(void);

/* Packet routing */
int vpn_route_packet(struct sk_buff *skb);
int vpn_should_route(uint32_t dest_ip, uint16_t dest_port);

/* WireGuard specific */
int vpn_wireguard_init(void);
int vpn_wireguard_handshake(void);
int vpn_wireguard_encrypt(struct sk_buff *skb);
int vpn_wireguard_decrypt(struct sk_buff *skb);

#endif /* LETHAL_NET_VPN_VPN_H */