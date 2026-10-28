/*
 * Lethal Kernel - Tor Integration
 *
 * v5.0: Native Tor integration for anonymous networking.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_TOR_TOR_H
#define LETHAL_NET_TOR_TOR_H

#include <stdint.h>
#include <stddef.h>
#include "../core/net.h"
#include "../core/sk_buff.h"

/* Tor states */
#define TOR_STATE_OFF          0
#define TOR_STATE_INIT         1
#define TOR_STATE_CONNECTING   2
#define TOR_STATE_HANDSHAKE    3
#define TOR_STATE_ESTABLISHED  4
#define TOR_STATE_ERROR        5

/* Tor circuit states */
#define TOR_CIRCUIT_IDLE       0
#define TOR_CIRCUIT_BUILDING   1
#define TOR_CIRCUIT_OPEN       2
#define TOR_CIRCUIT_CLOSING    3

/* Tor circuit structure */
struct tor_circuit {
    uint32_t id;
    uint32_t state;
    uint32_t path[3];          /* 3 nodes: guard, middle, exit */
    uint32_t path_len;
    uint8_t created;
    uint32_t bytes_sent;
    uint32_t bytes_received;
    void *next;
};

/* Tor stream structure */
struct tor_stream {
    uint32_t stream_id;
    uint32_t circuit_id;
    uint32_t dest_ip;
    uint16_t dest_port;
    uint8_t connected;
    uint8_t closed;
    void *next;
};

/* Tor configuration */
struct tor_config {
    uint8_t enabled;
    uint8_t auto_start;
    uint8_t use_bridges;
    char bridges[3][64];
    char socks_proxy[64];
    uint16_t socks_port;
    uint8_t use_socks5;
    uint8_t strict_nodes;
    uint8_t max_circuits;
    uint8_t circuit_rotation_hours;
    uint32_t timeout_ms;
};

/* Tor status */
struct tor_status {
    uint8_t state;
    uint32_t circuit_count;
    uint32_t stream_count;
    uint32_t bytes_sent;
    uint32_t bytes_received;
    uint32_t handshake_time_ms;
    char last_error[64];
};

/* ============================================================================
 * Function Prototypes
 * ============================================================================ */

/* Initialization */
int tor_init(void);
int tor_shutdown(void);

/* Control functions */
int tor_start(void);
int tor_stop(void);
int tor_restart(void);
int tor_is_running(void);
int tor_get_status(struct tor_status *status);

/* Circuit management */
int tor_circuit_create(void);
int tor_circuit_destroy(uint32_t circuit_id);
int tor_circuit_get_state(uint32_t circuit_id);
uint32_t tor_circuit_get_count(void);

/* Stream management */
int tor_stream_open(uint32_t circuit_id, uint32_t dest_ip, uint16_t dest_port);
int tor_stream_close(uint32_t stream_id);
int tor_stream_send(uint32_t stream_id, const void *data, size_t len);
int tor_stream_recv(uint32_t stream_id, void *buffer, size_t *len);

/* Packet routing */
int tor_route_packet(struct sk_buff *skb);
int tor_should_route(uint32_t dest_ip, uint16_t dest_port);

/* Configuration */
int tor_set_config(struct tor_config *config);
int tor_get_config(struct tor_config *config);
int tor_add_bridge(const char *address);
int tor_remove_bridge(const char *address);

/* Proxy support */
int tor_set_socks_proxy(const char *proxy, uint16_t port);
int tor_connect_via_socks(uint32_t dest_ip, uint16_t dest_port);

#endif /* LETHAL_NET_TOR_TOR_H */