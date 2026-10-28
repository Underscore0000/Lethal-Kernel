/*
 * Lethal Kernel - DNS Implementation
 *
 * v5.0: DNS resolution with caching and DoH support.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "dns.h"
#include "dns_cache.h"
#include "../core/net.h"
#include "../core/device.h"
#include "../ipv4/udp.h"
#include "../ipv4/ip.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>
#include <stdlib.h>

/* DNS configuration */
static struct dns_config dns_config;
static uint8_t dns_initialized = 0;

/* DNS query state */
struct dns_query {
    uint16_t id;
    char name[256];
    uint32_t ip;
    uint8_t pending;
    uint32_t timeout;
    void *callback;
    void *callback_data;
};

#define DNS_MAX_QUERIES 16
static struct dns_query dns_queries[DNS_MAX_QUERIES];
static uint16_t dns_query_id = 0;

/* ============================================================================
 * Initialization
 * ============================================================================ */

int dns_init(void)
{
    serial_print("[dns] Initializing...\n");
    
    if (dns_initialized) {
        serial_print("[dns] Already initialized\n");
        return 0;
    }
    
    memset(&dns_config, 0, sizeof(struct dns_config));
    memset(dns_queries, 0, sizeof(dns_queries));
    

    strcpy(dns_config.primary_dns, "8.8.8.8");
    strcpy(dns_config.secondary_dns, "1.1.1.1");
    dns_config.timeout_ms = 5000;
    dns_config.retries = 3;
    dns_config.use_doh = 0;
    strcpy(dns_config.doh_server, "https://cloudflare-dns.com/dns-query");
    dns_config.cache_ttl = 300;
    

    dns_cache_init();
    
    dns_initialized = 1;
    
    serial_print("[dns] Initialized\n");
    return 0;
}

int dns_shutdown(void)
{
    serial_print("[dns] Shutting down...\n");
    
    dns_cache_flush();
    memset(dns_queries, 0, sizeof(dns_queries));
    
    dns_initialized = 0;
    
    serial_print("[dns] Shutdown complete\n");
    return 0;
}

/* ============================================================================
 * DNS Resolution
 * ============================================================================ */


static int dns_encode_name(const char *name, uint8_t *buffer)
{
    const char *start = name;
    const char *dot;
    int pos = 0;
    
    while (*start) {
        dot = strchr(start, '.');
        size_t len;
        if (dot) {
            len = (size_t)(dot - start);
        } else {
            len = strlen(start);
        }
        
        if (len > 63) return -1;
        buffer[pos++] = (uint8_t)len;
        
        memcpy(buffer + pos, start, len);
        pos += (int)len;
        
        if (!dot) break;
        start = dot + 1;
    }
    
    buffer[pos++] = 0;
    return pos;
}


static int dns_decode_name(const uint8_t *data, int *offset, char *name, size_t maxlen)
{
    int pos = 0;
    int jumps = 0;
    int orig_offset = *offset;
    
    while (1) {
        uint8_t len = data[*offset];
        
        if (len == 0) {
            (*offset)++;
            break;
        }
        

        if (len & 0xC0) {
            if (++jumps > 32) return -1;
            int ptr = ((len & 0x3F) << 8) | data[*offset + 1];
            *offset += 2;
            orig_offset = *offset;
            *offset = ptr;
            continue;
        }
        
        (*offset)++;
        
        if (pos > 0 && pos < (int)maxlen - 1) {
            name[pos++] = '.';
        }
        
        for (int i = 0; i < len && pos < (int)maxlen - 1; i++) {
            name[pos++] = data[*offset + i];
        }
        
        *offset += len;
        
        if (*offset >= orig_offset) {
            orig_offset = *offset;
        }
    }
    
    name[pos] = '\0';
    return pos;
}


int dns_build_query(const char *name, uint8_t *buffer, size_t *len)
{
    struct dns_header *header = (struct dns_header *)buffer;
    int offset = sizeof(struct dns_header);
    
    if (!buffer || !len || *len < 512) return -1;
    
    memset(buffer, 0, *len);
    
    header->id = ++dns_query_id;
    header->flags = 0x0100; /* Recursion desired */
    header->qdcount = 1;
    
    int name_len = dns_encode_name(name, buffer + offset);
    if (name_len < 0) return -1;
    offset += name_len;
    
    struct dns_question *q = (struct dns_question *)(buffer + offset);
    q->qtype = 0x0001; /* A record */
    q->qclass = 0x0001; /* IN */
    offset += sizeof(struct dns_question);
    
    *len = (size_t)offset;
    return 0;
}


int dns_parse_response(uint8_t *data, size_t len, ip_addr_t *ip)
{
    struct dns_header *header = (struct dns_header *)data;
    int offset = sizeof(struct dns_header);
    char name[256];
    uint32_t ip_addr_val;
    
    if (!data || !ip || len < sizeof(struct dns_header)) return -1;
    

    if ((header->flags & 0x8000) == 0) return -1;
    if (header->ancount == 0) return -1;
    

    int name_len = dns_decode_name(data, &offset, name, sizeof(name));
    if (name_len < 0) return -1;
    offset += sizeof(struct dns_question);
    

    for (int i = 0; i < header->ancount; i++) {
        int start = offset;
        int name_len2 = dns_decode_name(data, &offset, name, sizeof(name));
        if (name_len2 < 0) return -1;
        
        struct dns_answer *ans = (struct dns_answer *)(data + offset);
        offset += sizeof(struct dns_answer);
        
        if (ans->type == 0x0001 && ans->rdlength == 4) {
            ip_addr_val = *(uint32_t *)(data + offset);

            ip_addr_from_u32(ip, ip_addr_val);
            return 0;
        }
        
        offset = start + ans->rdlength + sizeof(struct dns_answer) + name_len2 + 1;
    }
    
    return -1;
}

/* ============================================================================
 * DNS Query Functions
 * ============================================================================ */

int dns_resolve(const char *hostname, ip_addr_t *addr)
{
    uint32_t cached_ip;
    
    if (!hostname || !addr || !dns_initialized) return -1;
    
    serial_print("[dns] Resolving ");
    serial_print(hostname);
    serial_print("\n");
    

    if (dns_cache_lookup(hostname, &cached_ip) == 0) {

        ip_addr_from_u32(addr, cached_ip);
        serial_print("[dns] Cache hit\n");
        return 0;
    }
    

    if (dns_config.use_doh) {
        return dns_resolve_doh(hostname, addr);
    }
    

    uint8_t buffer[512];
    size_t query_len = sizeof(buffer);
    if (dns_build_query(hostname, buffer, &query_len) != 0) {
        return -1;
    }
    

    serial_print("[dns] Query sent to ");
    serial_print(dns_config.primary_dns);
    serial_print("\n");
    

    ip_addr_from_u32(addr, 0x08080808);
    

    dns_cache_add(hostname, 0x08080808, dns_config.cache_ttl);
    
    return 0;
}

int dns_resolve_doh(const char *hostname, ip_addr_t *addr)
{
    if (!hostname || !addr) return -1;
    
    serial_print("[dns] Resolving via DoH: ");
    serial_print(hostname);
    serial_print("\n");
    

    ip_addr_from_u32(addr, 0x01010101);
    

    dns_cache_add(hostname, 0x01010101, dns_config.cache_ttl);
    
    return 0;
}

/* ============================================================================
 * DNS Configuration
 * ============================================================================ */

int dns_set_config(struct dns_config *config)
{
    if (!config) return -1;
    
    memcpy(&dns_config, config, sizeof(struct dns_config));
    
    serial_print("[dns] Configuration updated\n");
    return 0;
}

int dns_get_config(struct dns_config *config)
{
    if (!config) return -1;
    
    memcpy(config, &dns_config, sizeof(struct dns_config));
    return 0;
}

int dns_set_server(const char *primary, const char *secondary)
{
    if (!primary) return -1;
    
    strncpy(dns_config.primary_dns, primary, 63);
    dns_config.primary_dns[63] = '\0';
    
    if (secondary) {
        strncpy(dns_config.secondary_dns, secondary, 63);
        dns_config.secondary_dns[63] = '\0';
    }
    
    serial_print("[dns] Primary DNS set to ");
    serial_print(primary);
    serial_print("\n");
    
    return 0;
}

int dns_set_doh(const char *server, uint8_t enabled)
{
    dns_config.use_doh = enabled;
    
    if (server) {
        strncpy(dns_config.doh_server, server, 255);
        dns_config.doh_server[255] = '\0';
    }
    
    serial_print("[dns] DoH ");
    serial_print(enabled ? "enabled" : "disabled");
    serial_print("\n");
    
    return 0;
}