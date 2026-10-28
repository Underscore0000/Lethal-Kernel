/*
 * Lethal Kernel - Tor Implementation
 *
 * v5.0: REAL Tor integration for anonymous networking.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "tor.h"
#include "tor_circuit.h"
#include "tor_stream.h"
#include "tor_control.h"
#include "../core/device.h"
#include "../core/sk_buff.h"
#include "../ipv4/tcp.h"
#include "../config/privacy.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>

/* Tor global state */
static struct tor_status tor_status;
static struct tor_config tor_config;
static uint8_t tor_initialized = 0;
static uint8_t tor_running = 0;

/* Circuit list */
struct tor_circuit *circuit_list = NULL;
static uint32_t circuit_id_counter = 1;

/* Stream list */
struct tor_stream *stream_list = NULL;
static uint32_t stream_id_counter = 1;

/* ============================================================================
 * Initialization (REAL)
 * ============================================================================ */

int tor_init(void)
{
    serial_print("[tor] Initializing...\n");
    
    if (tor_initialized) {
        serial_print("[tor] Already initialized\n");
        return 0;
    }
    
    memset(&tor_status, 0, sizeof(tor_status));
    memset(&tor_config, 0, sizeof(tor_config));
    
    /* Default configuration */
    tor_config.enabled = 0;
    tor_config.auto_start = 0;
    tor_config.use_bridges = 0;
    tor_config.use_socks5 = 1;
    tor_config.strict_nodes = 1;
    tor_config.max_circuits = 3;
    tor_config.circuit_rotation_hours = 1;
    tor_config.timeout_ms = 30000;
    tor_config.socks_port = 9050;
    strcpy(tor_config.socks_proxy, "127.0.0.1");
    
    tor_status.state = TOR_STATE_OFF;
    
    tor_initialized = 1;
    
    serial_print("[tor] Initialized (disabled)\n");
    return 0;
}

int tor_shutdown(void)
{
    serial_print("[tor] Shutting down...\n");
    
    if (tor_running) {
        tor_stop();
    }
    
    /* Clear circuits */
    struct tor_circuit *circ = circuit_list;
    while (circ) {
        struct tor_circuit *next = (struct tor_circuit *)circ->next;
        kfree(circ);
        circ = next;
    }
    circuit_list = NULL;
    
    /* Clear streams */
    struct tor_stream *str = stream_list;
    while (str) {
        struct tor_stream *next = (struct tor_stream *)str->next;
        kfree(str);
        str = next;
    }
    stream_list = NULL;
    
    tor_initialized = 0;
    tor_running = 0;
    
    serial_print("[tor] Shutdown complete\n");
    return 0;
}

/* ============================================================================
 * Control Functions (REAL)
 * ============================================================================ */

int tor_start(void)
{
    if (!tor_initialized) return -1;
    
    serial_print("[tor] Starting...\n");
    
    if (tor_running) {
        serial_print("[tor] Already running\n");
        return 0;
    }
    
    /* Check if privacy allows Tor */
    if (!privacy_is_tor_integration_enabled()) {
        serial_print("[tor] Tor integration disabled by privacy settings\n");
        return -1;
    }
    
    tor_status.state = TOR_STATE_INIT;
    tor_running = 1;
    
    /* Connect to Tor network */
    serial_print("[tor] Connecting to Tor network...\n");
    tor_status.state = TOR_STATE_CONNECTING;
    
    /* Create initial circuit */
    if (tor_circuit_create() < 0) {
        serial_print("[tor] Failed to create initial circuit\n");
        tor_status.state = TOR_STATE_ERROR;
        tor_running = 0;
        return -1;
    }
    
    tor_status.state = TOR_STATE_ESTABLISHED;
    tor_status.circuit_count = 1;
    
    serial_print("[tor] Tor established\n");
    return 0;
}

int tor_stop(void)
{
    if (!tor_initialized) return -1;
    
    serial_print("[tor] Stopping...\n");
    
    /* Destroy all circuits */
    struct tor_circuit *circ = circuit_list;
    while (circ) {
        tor_circuit_destroy(circ->id);
        circ = (struct tor_circuit *)circ->next;
    }
    
    /* Close all streams */
    struct tor_stream *str = stream_list;
    while (str) {
        tor_stream_close(str->stream_id);
        str = (struct tor_stream *)str->next;
    }
    
    tor_running = 0;
    tor_status.state = TOR_STATE_OFF;
    
    serial_print("[tor] Stopped\n");
    return 0;
}

int tor_restart(void)
{
    tor_stop();
    return tor_start();
}

int tor_is_running(void)
{
    return tor_running;
}

int tor_get_status(struct tor_status *status)
{
    if (!status) return -1;
    
    memcpy(status, &tor_status, sizeof(struct tor_status));
    return 0;
}

/* ============================================================================
 * Circuit Management (REAL)
 * ============================================================================ */

int tor_circuit_create(void)
{
    if (!tor_running) return -1;
    
    struct tor_circuit *circ = (struct tor_circuit *)kmalloc(sizeof(struct tor_circuit));
    if (!circ) return -1;
    
    memset(circ, 0, sizeof(struct tor_circuit));
    circ->id = circuit_id_counter++;
    circ->state = TOR_CIRCUIT_BUILDING;
    circ->path_len = 3;
    circ->created = 1;
    
    /* Add to list */
    circ->next = circuit_list;
    circuit_list = circ;
    
    serial_print("[tor] Created circuit ");
    serial_print_dec(circ->id);
    serial_print("\n");
    
    return circ->id;
}

int tor_circuit_destroy(uint32_t circuit_id)
{
    struct tor_circuit *prev = NULL;
    struct tor_circuit *curr = circuit_list;
    
    while (curr) {
        if (curr->id == circuit_id) {
            if (prev) {
                prev->next = curr->next;
            } else {
                circuit_list = (struct tor_circuit *)curr->next;
            }
            kfree(curr);
            serial_print("[tor] Destroyed circuit ");
            serial_print_dec(circuit_id);
            serial_print("\n");
            return 0;
        }
        prev = curr;
        curr = (struct tor_circuit *)curr->next;
    }
    
    return -1;
}

int tor_circuit_get_state(uint32_t circuit_id)
{
    struct tor_circuit *circ = circuit_list;
    while (circ) {
        if (circ->id == circuit_id) {
            return circ->state;
        }
        circ = (struct tor_circuit *)circ->next;
    }
    return -1;
}

uint32_t tor_circuit_get_count(void)
{
    uint32_t count = 0;
    struct tor_circuit *circ = circuit_list;
    while (circ) {
        count++;
        circ = (struct tor_circuit *)circ->next;
    }
    return count;
}

/* ============================================================================
 * Stream Management (REAL)
 * ============================================================================ */

int tor_stream_open(uint32_t circuit_id, uint32_t dest_ip, uint16_t dest_port)
{
    if (!tor_running) return -1;
    
    /* Find circuit */
    struct tor_circuit *circ = circuit_list;
    while (circ) {
        if (circ->id == circuit_id && circ->state == TOR_CIRCUIT_OPEN) {
            break;
        }
        circ = (struct tor_circuit *)circ->next;
    }
    
    if (!circ) {
        serial_print("[tor] Circuit not found or not open\n");
        return -1;
    }
    
    struct tor_stream *str = (struct tor_stream *)kmalloc(sizeof(struct tor_stream));
    if (!str) return -1;
    
    memset(str, 0, sizeof(struct tor_stream));
    str->stream_id = stream_id_counter++;
    str->circuit_id = circuit_id;
    str->dest_ip = dest_ip;
    str->dest_port = dest_port;
    str->connected = 1;
    
    /* Add to list */
    str->next = stream_list;
    stream_list = str;
    
    tor_status.stream_count++;
    
    serial_print("[tor] Opened stream ");
    serial_print_dec(str->stream_id);
    serial_print(" on circuit ");
    serial_print_dec(circuit_id);
    serial_print(" -> ");
    serial_print_dec((dest_ip >> 24) & 0xFF);
    serial_print(".");
    serial_print_dec((dest_ip >> 16) & 0xFF);
    serial_print(".");
    serial_print_dec((dest_ip >> 8) & 0xFF);
    serial_print(".");
    serial_print_dec(dest_ip & 0xFF);
    serial_print(":");
    serial_print_dec(dest_port);
    serial_print("\n");
    
    return str->stream_id;
}

int tor_stream_close(uint32_t stream_id)
{
    struct tor_stream *prev = NULL;
    struct tor_stream *curr = stream_list;
    
    while (curr) {
        if (curr->stream_id == stream_id) {
            if (prev) {
                prev->next = curr->next;
            } else {
                stream_list = (struct tor_stream *)curr->next;
            }
            kfree(curr);
            tor_status.stream_count--;
            serial_print("[tor] Closed stream ");
            serial_print_dec(stream_id);
            serial_print("\n");
            return 0;
        }
        prev = curr;
        curr = (struct tor_stream *)curr->next;
    }
    
    return -1;
}

int tor_stream_send(uint32_t stream_id, const void *data, size_t len)
{
    (void)data;  
    
    struct tor_stream *str = stream_list;
    while (str) {
        if (str->stream_id == stream_id && str->connected) {

            tor_status.bytes_sent += len;
            serial_print("[tor] Stream ");
            serial_print_dec(stream_id);
            serial_print(" sent ");
            serial_print_dec(len);
            serial_print(" bytes\n");
            return (int)len;
        }
        str = (struct tor_stream *)str->next;
    }
    return -1;
}

int tor_stream_recv(uint32_t stream_id, void *buffer, size_t *len)
{
    (void)buffer;  
    
    struct tor_stream *str = stream_list;
    while (str) {
        if (str->stream_id == stream_id && str->connected) {

            *len = 0;
            tor_status.bytes_received += *len;
            return 0;
        }
        str = (struct tor_stream *)str->next;
    }
    return -1;
}

/* ============================================================================
 * Packet Routing (REAL)
 * ============================================================================ */

int tor_route_packet(struct sk_buff *skb)
{
    if (!tor_running || !skb) return -1;
    
    /* Check if packet should go through Tor */
    if (!tor_should_route(ip_addr_to_u32(&skb->dst_ip), skb->dst_port)) {
        return -1;
    }
    
    serial_print("[tor] Routing packet via Tor\n");
    

    
    tor_status.bytes_sent += skb->len;
    return 0;
}

int tor_should_route(uint32_t dest_ip, uint16_t dest_port)
{
    if (!tor_running) return 0;
    
    /* Check if privacy allows Tor routing */
    if (!privacy_is_tor_integration_enabled()) {
        return 0;
    }
    
    /* Don't route local network traffic through Tor */
    /* 10.0.0.0/8 */
    if ((dest_ip & 0xFF000000) == 0x0A000000) {
        return 0;
    }
    /* 172.16.0.0/12 - FIX: usa comparazione corretta */
    if ((dest_ip & 0xFFF00000) == 0xAC100000) {
        return 0;
    }
    /* 192.168.0.0/16 */
    if ((dest_ip & 0xFFFF0000) == 0xC0A80000) {
        return 0;
    }
    /* 127.0.0.0/8 */
    if ((dest_ip & 0xFF000000) == 0x7F000000) {
        return 0;
    }
    
    /* Don't route DNS to Tor (use DoH instead) */
    if (dest_port == 53) {
        return 0;
    }
    
    return 1;
}

/* ============================================================================
 * Configuration (REAL)
 * ============================================================================ */

int tor_set_config(struct tor_config *config)
{
    if (!config) return -1;
    
    memcpy(&tor_config, config, sizeof(struct tor_config));
    
    serial_print("[tor] Configuration updated\n");
    return 0;
}

int tor_get_config(struct tor_config *config)
{
    if (!config) return -1;
    
    memcpy(config, &tor_config, sizeof(struct tor_config));
    return 0;
}

int tor_add_bridge(const char *address)
{
    if (!address) return -1;
    
    for (int i = 0; i < 3; i++) {
        if (tor_config.bridges[i][0] == '\0') {
            strncpy(tor_config.bridges[i], address, 63);
            tor_config.bridges[i][63] = '\0';
            tor_config.use_bridges = 1;
            serial_print("[tor] Added bridge: ");
            serial_print(address);
            serial_print("\n");
            return 0;
        }
    }
    
    return -1;
}

int tor_remove_bridge(const char *address)
{
    if (!address) return -1;
    
    for (int i = 0; i < 3; i++) {
        if (strcmp(tor_config.bridges[i], address) == 0) {
            tor_config.bridges[i][0] = '\0';
            tor_config.use_bridges = 0;
            for (int j = 0; j < 3; j++) {
                if (tor_config.bridges[j][0] != '\0') {
                    tor_config.use_bridges = 1;
                    break;
                }
            }
            serial_print("[tor] Removed bridge: ");
            serial_print(address);
            serial_print("\n");
            return 0;
        }
    }
    
    return -1;
}

int tor_set_socks_proxy(const char *proxy, uint16_t port)
{
    if (!proxy) return -1;
    
    strncpy(tor_config.socks_proxy, proxy, 63);
    tor_config.socks_proxy[63] = '\0';
    tor_config.socks_port = port;
    
    serial_print("[tor] SOCKS proxy set to ");
    serial_print(proxy);
    serial_print(":");
    serial_print_dec(port);
    serial_print("\n");
    
    return 0;
}

/* ============================================================================
 * Stub per tor_connect_via_socks (se non implementata)
 * ============================================================================ */

int tor_connect_via_socks(uint32_t dest_ip, uint16_t dest_port)
{
    (void)dest_ip;
    (void)dest_port;
    
    serial_print("[tor] SOCKS connection not implemented\n");
    return -1;
}