/*
 * Lethal Kernel - Tor Control Interface
 *
 * v5.0: Control interface for Tor operations (start/stop/status).
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "tor.h"
#include "tor_control.h"
#include "tor_circuit.h"
#include "tor_stream.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include "../../fs/vfs.h"
#include "../config/privacy.h"
#include <string.h>
#include <stdio.h>     
#include <stdlib.h>    

/* ============================================================================
 * Control Commands (REAL)
 * ============================================================================ */

int tor_control_start(void)
{
    serial_print("[tor_control] Starting Tor...\n");
    
    /* Check if Tor is already running */
    if (tor_is_running()) {
        serial_print("[tor_control] Tor is already running\n");
        return 0;
    }
    
    /* Check if Tor is enabled in config */
    struct tor_config config;
    tor_get_config(&config);
    
    if (!config.enabled) {
        serial_print("[tor_control] Tor is disabled in configuration\n");
        return -1;
    }
    
    /* Start Tor */
    return tor_start();
}

int tor_control_stop(void)
{
    serial_print("[tor_control] Stopping Tor...\n");
    
    if (!tor_is_running()) {
        serial_print("[tor_control] Tor is not running\n");
        return 0;
    }
    
    return tor_stop();
}

int tor_control_restart(void)
{
    serial_print("[tor_control] Restarting Tor...\n");
    
    tor_stop();
    return tor_start();
}

int tor_control_status(void)
{
    struct tor_status status;
    
    tor_get_status(&status);
    
    serial_print("\n");
    serial_print("=== Tor Status ===\n");
    serial_print("State: ");
    switch(status.state) {
        case TOR_STATE_OFF:         serial_print("OFF\n"); break;
        case TOR_STATE_INIT:        serial_print("INITIALIZING\n"); break;
        case TOR_STATE_CONNECTING:  serial_print("CONNECTING\n"); break;
        case TOR_STATE_HANDSHAKE:   serial_print("HANDSHAKE\n"); break;
        case TOR_STATE_ESTABLISHED: serial_print("ESTABLISHED\n"); break;
        case TOR_STATE_ERROR:       serial_print("ERROR\n"); break;
        default:                    serial_print("UNKNOWN\n"); break;
    }
    serial_print("Circuits: ");
    serial_print_dec(status.circuit_count);
    serial_print("\n");
    serial_print("Streams: ");
    serial_print_dec(status.stream_count);
    serial_print("\n");
    serial_print("Bytes sent: ");
    serial_print_dec(status.bytes_sent);
    serial_print("\n");
    serial_print("Bytes received: ");
    serial_print_dec(status.bytes_received);
    serial_print("\n");
    serial_print("Handshake time: ");
    serial_print_dec(status.handshake_time_ms);
    serial_print("ms\n");
    if (status.last_error[0] != '\0') {
        serial_print("Last error: ");
        serial_print(status.last_error);
        serial_print("\n");
    }
    serial_print("\n");
    
    return 0;
}

/* ============================================================================
 * Control Configuration (REAL)
 * ============================================================================ */

int tor_control_set_enabled(uint8_t enabled)
{
    struct tor_config config;
    tor_get_config(&config);
    config.enabled = enabled;
    tor_set_config(&config);
    
    serial_print("[tor_control] Tor ");
    serial_print(enabled ? "enabled" : "disabled");
    serial_print("\n");
    
    /* If enabled and privacy allows, start Tor */
    if (enabled && privacy_is_tor_integration_enabled()) {
        tor_control_start();
    } else if (!enabled) {
        tor_control_stop();
    }
    
    return 0;
}

int tor_control_set_auto_start(uint8_t auto_start)
{
    struct tor_config config;
    tor_get_config(&config);
    config.auto_start = auto_start;
    tor_set_config(&config);
    
    serial_print("[tor_control] Auto-start ");
    serial_print(auto_start ? "enabled" : "disabled");
    serial_print("\n");
    
    return 0;
}

int tor_control_set_timeout(uint32_t timeout_ms)
{
    struct tor_config config;
    tor_get_config(&config);
    config.timeout_ms = timeout_ms;
    tor_set_config(&config);
    
    serial_print("[tor_control] Timeout set to ");
    serial_print_dec(timeout_ms);
    serial_print("ms\n");
    
    return 0;
}

int tor_control_set_max_circuits(uint8_t max_circuits)
{
    struct tor_config config;
    tor_get_config(&config);
    config.max_circuits = max_circuits;
    tor_set_config(&config);
    
    serial_print("[tor_control] Max circuits set to ");
    serial_print_dec(max_circuits);
    serial_print("\n");
    
    return 0;
}

/* ============================================================================
 * Control Utility Functions (REAL)
 * ============================================================================ */

int tor_control_new_circuit(void)
{
    serial_print("[tor_control] Creating new circuit...\n");
    
    if (!tor_is_running()) {
        serial_print("[tor_control] Tor is not running\n");
        return -1;
    }
    
    int circuit_id = tor_circuit_create();
    if (circuit_id < 0) {
        serial_print("[tor_control] Failed to create circuit\n");
        return -1;
    }
    
    serial_print("[tor_control] Circuit ");
    serial_print_dec(circuit_id);
    serial_print(" created\n");
    
    return circuit_id;
}

int tor_control_close_circuit(uint32_t circuit_id)
{
    serial_print("[tor_control] Closing circuit ");
    serial_print_dec(circuit_id);
    serial_print("\n");
    
    return tor_circuit_close(circuit_id);
}

int tor_control_list_circuits(void)
{
    serial_print("\n");
    serial_print("=== Tor Circuits ===\n");
    
    struct tor_circuit *circ = circuit_list;
    if (!circ) {
        serial_print("No circuits\n");
        serial_print("\n");
        return 0;
    }
    
    uint32_t count = 0;
    while (circ) {
        serial_print("Circuit ");
        serial_print_dec(circ->id);
        serial_print(": ");
        switch(circ->state) {
            case TOR_CIRCUIT_IDLE:      serial_print("IDLE\n"); break;
            case TOR_CIRCUIT_BUILDING:  serial_print("BUILDING\n"); break;
            case TOR_CIRCUIT_OPEN:      serial_print("OPEN\n"); break;
            case TOR_CIRCUIT_CLOSING:   serial_print("CLOSING\n"); break;
            default:                    serial_print("UNKNOWN\n"); break;
        }
        serial_print("  Path: ");

        for (uint32_t i = 0; i < circ->path_len; i++) {
            uint32_t ip = circ->path[i];
            serial_print_dec((ip >> 24) & 0xFF);
            serial_print(".");
            serial_print_dec((ip >> 16) & 0xFF);
            serial_print(".");
            serial_print_dec((ip >> 8) & 0xFF);
            serial_print(".");
            serial_print_dec(ip & 0xFF);
            if (i < circ->path_len - 1) serial_print(" -> ");
        }
        serial_print("\n");
        serial_print("  Bytes: ");
        serial_print_dec(circ->bytes_sent + circ->bytes_received);
        serial_print("\n");
        count++;
        circ = (struct tor_circuit *)circ->next;
    }
    
    serial_print("\n");
    return 0;
}

int tor_control_list_streams(void)
{
    serial_print("\n");
    serial_print("=== Tor Streams ===\n");
    
    struct tor_stream *str = stream_list;
    if (!str) {
        serial_print("No streams\n");
        serial_print("\n");
        return 0;
    }
    
    while (str) {
        serial_print("Stream ");
        serial_print_dec(str->stream_id);
        serial_print(": circuit ");
        serial_print_dec(str->circuit_id);
        serial_print(" -> ");
        serial_print_dec((str->dest_ip >> 24) & 0xFF);
        serial_print(".");
        serial_print_dec((str->dest_ip >> 16) & 0xFF);
        serial_print(".");
        serial_print_dec((str->dest_ip >> 8) & 0xFF);
        serial_print(".");
        serial_print_dec(str->dest_ip & 0xFF);
        serial_print(":");
        serial_print_dec(str->dest_port);
        serial_print(" ");
        serial_print(str->connected ? "connected" : "closed");
        serial_print("\n");
        
        str = (struct tor_stream *)str->next;
    }
    
    serial_print("\n");
    return 0;
}

/* ============================================================================
 * Control File Interface (REAL)
 * ============================================================================ */

int tor_control_load_config(const char *path)
{
    if (!path) return -1;
    
    serial_print("[tor_control] Loading config from ");
    serial_print(path);
    serial_print("\n");
    
    void *file = NULL;
    if (vfs_open(path, VFS_O_READ, &file) != 0) {
        serial_print("[tor_control] Config file not found\n");
        return -1;
    }
    
    long size = vfs_size(file);
    if (size <= 0 || size > 4096) {
        vfs_close(file);
        return -1;
    }
    
    char *buffer = (char *)kmalloc((size_t)size + 1);
    if (!buffer) {
        vfs_close(file);
        return -1;
    }
    
    long read = vfs_read(file, buffer, (size_t)size);
    vfs_close(file);
    
    if (read <= 0) {
        kfree(buffer);
        return -1;
    }
    
    buffer[read] = '\0';
    
    /* Parse config file */
    char *line = strtok(buffer, "\n");
    while (line) {
        if (line[0] == '#' || line[0] == '\0') {
            line = strtok(NULL, "\n");
            continue;
        }
        
        char *eq = strchr(line, '=');
        if (!eq) {
            line = strtok(NULL, "\n");
            continue;
        }
        
        *eq = '\0';
        char *key = line;
        char *value = eq + 1;
        
        while (*key == ' ') key++;
        while (*value == ' ') value++;
        
        size_t key_len = strlen(key);
        while (key_len > 0 && key[key_len-1] == ' ') {
            key[key_len-1] = '\0';
            key_len--;
        }
        size_t val_len = strlen(value);
        while (val_len > 0 && value[val_len-1] == ' ') {
            value[val_len-1] = '\0';
            val_len--;
        }
        
        if (strcmp(key, "enabled") == 0) {
            int val = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            tor_control_set_enabled((uint8_t)val);
        } else if (strcmp(key, "auto_start") == 0) {
            int val = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            tor_control_set_auto_start((uint8_t)val);
        } else if (strcmp(key, "timeout") == 0) {
            uint32_t val = 0;
            sscanf(value, "%u", &val);
            tor_control_set_timeout(val);
        } else if (strcmp(key, "max_circuits") == 0) {
            uint8_t val = 0;
            sscanf(value, "%hhu", &val);
            tor_control_set_max_circuits(val);
        } else if (strcmp(key, "bridge") == 0) {
            tor_add_bridge(value);
        } else if (strcmp(key, "socks_proxy") == 0) {
            char *comma = strchr(value, ':');
            if (comma) {
                *comma = '\0';
                uint16_t port = 0;
                sscanf(comma + 1, "%hu", &port);
                tor_set_socks_proxy(value, port);
            }
        }
        
        line = strtok(NULL, "\n");
    }
    
    kfree(buffer);
    
    serial_print("[tor_control] Config loaded\n");
    return 0;
}

int tor_control_save_config(const char *path)
{
    if (!path) return -1;
    
    char buffer[1024];
    int pos = 0;
    
    struct tor_config config;
    tor_get_config(&config);
    
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "# Tor Configuration\n");
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "# Generated automatically\n\n");
    
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "enabled = %s\n", config.enabled ? "true" : "false");
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "auto_start = %s\n", config.auto_start ? "true" : "false");
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "timeout = %u\n", config.timeout_ms);
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "max_circuits = %u\n", config.max_circuits);
    
    for (int i = 0; i < 3; i++) {
        if (config.bridges[i][0] != '\0') {
            pos += snprintf(buffer + pos, sizeof(buffer) - pos,
                "bridge = %s\n", config.bridges[i]);
        }
    }
    
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "socks_proxy = %s:%u\n", config.socks_proxy, config.socks_port);
    
    /* Write to file */
    void *file = NULL;
    if (vfs_open(path, VFS_O_WRITE | VFS_O_CREAT, &file) != 0) {
        serial_print("[tor_control] Failed to save config\n");
        return -1;
    }
    
    vfs_write(file, buffer, (size_t)pos);
    vfs_close(file);
    
    serial_print("[tor_control] Config saved to ");
    serial_print(path);
    serial_print("\n");
    
    return 0;
}