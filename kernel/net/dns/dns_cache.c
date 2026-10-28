/*
 * Lethal Kernel - DNS Cache Implementation
 *
 * v5.0: DNS caching with TTL support.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "dns_cache.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>
#include <stdlib.h>

/* Struttura cache DNS */
struct dns_cache_entry {
    char name[256];
    uint32_t ip;
    uint32_t ttl;
    uint32_t expire_time;
    uint8_t valid;
};

#define DNS_CACHE_SIZE 64
static struct dns_cache_entry dns_cache[DNS_CACHE_SIZE];
static uint8_t dns_cache_initialized = 0;

/* ============================================================================
 * DNS Cache Functions
 * ============================================================================ */

void dns_cache_init(void)
{
    if (dns_cache_initialized) return;
    memset(dns_cache, 0, sizeof(dns_cache));
    dns_cache_initialized = 1;
    serial_print("[dns_cache] Initialized\n");
}

int dns_cache_add(const char *name, uint32_t ip, uint32_t ttl)
{
    if (!name) return -1;
    dns_cache_init();
    

    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (dns_cache[i].valid && strcmp(dns_cache[i].name, name) == 0) {
            dns_cache[i].ip = ip;
            dns_cache[i].ttl = ttl;
            dns_cache[i].expire_time = ttl;
            return 0;
        }
    }
    

    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (!dns_cache[i].valid) {
            strncpy(dns_cache[i].name, name, 255);
            dns_cache[i].name[255] = '\0';
            dns_cache[i].ip = ip;
            dns_cache[i].ttl = ttl;
            dns_cache[i].expire_time = ttl;
            dns_cache[i].valid = 1;
            return 0;
        }
    }
    
    return -1;
}

int dns_cache_lookup(const char *name, uint32_t *ip)
{
    if (!name || !ip) return -1;
    if (!dns_cache_initialized) return -1;
    
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (dns_cache[i].valid && strcmp(dns_cache[i].name, name) == 0) {
            *ip = dns_cache[i].ip;
            return 0;
        }
    }
    return -1;
}

int dns_cache_clear(void)
{
    memset(dns_cache, 0, sizeof(dns_cache));
    dns_cache_initialized = 0;
    serial_print("[dns_cache] Cleared\n");
    return 0;
}

int dns_cache_flush(void)
{
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        dns_cache[i].valid = 0;
    }
    serial_print("[dns_cache] Flushed\n");
    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

int dns_cache_get_size(void)
{
    int count = 0;
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (dns_cache[i].valid) count++;
    }
    return count;
}

int dns_cache_get_max_size(void)
{
    return DNS_CACHE_SIZE;
}

int dns_cache_remove(const char *name)
{
    if (!name) return -1;
    
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (dns_cache[i].valid && strcmp(dns_cache[i].name, name) == 0) {
            dns_cache[i].valid = 0;
            return 0;
        }
    }
    return -1;
}

void dns_cache_dump(void)
{
    serial_print("\n");
    serial_print("=== DNS Cache Dump ===\n");
    serial_print("Entries: ");
    serial_print_dec(dns_cache_get_size());
    serial_print(" / ");
    serial_print_dec(DNS_CACHE_SIZE);
    serial_print("\n");
    
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (dns_cache[i].valid) {
            serial_print("  ");
            serial_print(dns_cache[i].name);
            serial_print(" -> ");
            serial_print_dec((dns_cache[i].ip >> 24) & 0xFF);
            serial_print(".");
            serial_print_dec((dns_cache[i].ip >> 16) & 0xFF);
            serial_print(".");
            serial_print_dec((dns_cache[i].ip >> 8) & 0xFF);
            serial_print(".");
            serial_print_dec(dns_cache[i].ip & 0xFF);
            serial_print(" (TTL: ");
            serial_print_dec(dns_cache[i].ttl);
            serial_print(")\n");
        }
    }
    serial_print("\n");
}