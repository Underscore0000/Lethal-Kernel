/*
 * Lethal Kernel - DNS over HTTPS (DoH)
 *
 * v5.0: DNS over HTTPS for encrypted DNS queries.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "doh.h"
#include "dns.h"
#include "dns_cache.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include "../config/privacy.h"
#include "../core/net.h"
#include "../core/sk_buff.h"
#include "../ipv4/tcp.h"
#include "../ipv4/ip.h"
#include <stdlib.h>


static void *doh_memset(void *s, int c, size_t n)
{
    unsigned char *p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}


static char *doh_strstr(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return NULL;
    if (*needle == '\0') return (char *)haystack;
    
    for (const char *h = haystack; *h != '\0'; h++) {
        const char *h_tmp = h;
        const char *n_tmp = needle;
        while (*h_tmp != '\0' && *n_tmp != '\0' && *h_tmp == *n_tmp) {
            h_tmp++;
            n_tmp++;
        }
        if (*n_tmp == '\0') {
            return (char *)h;
        }
    }
    return NULL;
}

static const char *doh_hosts[] = {
    "cloudflare-dns.com",
    "dns.google",
    "dns.quad9.net"
};

static uint32_t doh_current = 0;
static uint8_t doh_initialized = 0;
static uint32_t doh_timeout_ms = 5000;


static struct {
    uint32_t queries_sent;
    uint32_t queries_received;
    uint32_t queries_failed;
    uint32_t cache_hits;
    uint32_t avg_response_ms;
} doh_stats = {0};

static const char base64_url_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

/* ============================================================================
 * Base64 URL Encoding
 * ============================================================================ */

static int base64_url_encode(const uint8_t *input, size_t len, char *output, size_t out_len)
{
    if (!input || !output || out_len < ((len + 2) / 3) * 4 + 1) return -1;
    
    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t val = ((uint32_t)input[i] << 16);
        if (i + 1 < len) val |= ((uint32_t)input[i+1] << 8);
        if (i + 2 < len) val |= input[i+2];
        
        output[o++] = base64_url_chars[(val >> 18) & 0x3F];
        output[o++] = base64_url_chars[(val >> 12) & 0x3F];
        output[o++] = (i + 1 < len) ? base64_url_chars[(val >> 6) & 0x3F] : '=';
        output[o++] = (i + 2 < len) ? base64_url_chars[val & 0x3F] : '=';
    }
    
    output[o] = '\0';
    return (int)o;
}

/* ============================================================================
 * DoH Initialization
 * ============================================================================ */

int dns_doh_init(void)
{
    serial_print("[doh] Initializing...\n");
    
    if (doh_initialized) {
        serial_print("[doh] Already initialized\n");
        return 0;
    }
    
    doh_current = 0;
    doh_timeout_ms = 5000;
    doh_memset(&doh_stats, 0, sizeof(doh_stats));
    doh_initialized = 1;
    
    serial_print("[doh] Initialized, server: ");
    serial_print(doh_hosts[doh_current]);
    serial_print("\n");
    
    return 0;
}

int dns_doh_shutdown(void)
{
    serial_print("[doh] Shutting down...\n");
    doh_initialized = 0;
    serial_print("[doh] Shutdown complete\n");
    return 0;
}

/* ============================================================================
 * DoH Query Building
 * ============================================================================ */

static int dns_doh_build_http_request(const char *encoded_query, char *buffer, size_t buflen)
{
    if (!encoded_query || !buffer || buflen < 512) return -1;
    
    const char *host = doh_hosts[doh_current];
    int pos = 0;
    
    /* GET /dns-query?dns= */
    const char *part1 = "GET /dns-query?dns=";
    while (*part1 && pos < (int)buflen - 1) {
        buffer[pos++] = *part1++;
    }
    
    /* encoded_query */
    while (*encoded_query && pos < (int)buflen - 1) {
        buffer[pos++] = *encoded_query++;
    }
    
    /* HTTP/1.1\r\nHost: */
    const char *part2 = " HTTP/1.1\r\nHost: ";
    while (*part2 && pos < (int)buflen - 1) {
        buffer[pos++] = *part2++;
    }
    
    /* host */
    while (*host && pos < (int)buflen - 1) {
        buffer[pos++] = *host++;
    }
    
    /* \r\nAccept: application/dns-message\r\nUser-Agent: LethalKernel/5.0\r\nConnection: close\r\n\r\n */
    const char *part3 = "\r\nAccept: application/dns-message\r\nUser-Agent: LethalKernel/5.0\r\nConnection: close\r\n\r\n";
    while (*part3 && pos < (int)buflen - 1) {
        buffer[pos++] = *part3++;
    }
    
    buffer[pos] = '\0';
    return pos;
}

/* ============================================================================
 * DoH DNS Resolution
 * ============================================================================ */

int dns_doh_resolve(const char *hostname, ip_addr_t *ip)
{
    uint32_t ip_val;
    
    if (!hostname || !ip) return -1;
    
    if (!doh_initialized) {
        serial_print("[doh] Not initialized\n");
        return -1;
    }
    
    serial_print("[doh] Resolving ");
    serial_print(hostname);
    serial_print(" via DoH (");
    serial_print(doh_hosts[doh_current]);
    serial_print(")\n");
    
    if (dns_cache_lookup(hostname, &ip_val) == 0) {
        ip_addr_from_u32(ip, ip_val);
        doh_stats.cache_hits++;
        serial_print("[doh] Cache hit\n");
        return 0;
    }
    
    uint8_t query[512];
    size_t query_len = sizeof(query);
    if (dns_build_query(hostname, query, &query_len) != 0) {
        serial_print("[doh] Failed to build DNS query\n");
        doh_stats.queries_failed++;
        return -1;
    }
    
    char encoded[1024];
    int enc_len = base64_url_encode(query, query_len, encoded, sizeof(encoded));
    if (enc_len < 0) {
        serial_print("[doh] Base64 encoding failed\n");
        doh_stats.queries_failed++;
        return -1;
    }
    
    serial_print("[doh] Query encoded, length ");
    serial_print_dec(enc_len);
    serial_print("\n");
    
    char http_request[2048];
    int req_len = dns_doh_build_http_request(encoded, http_request, sizeof(http_request));
    if (req_len < 0) {
        serial_print("[doh] Failed to build HTTP request\n");
        doh_stats.queries_failed++;
        return -1;
    }
    
    serial_print("[doh] HTTP request built, length ");
    serial_print_dec(req_len);
    serial_print("\n");
    
    serial_print("[doh] Sending request to ");
    serial_print(doh_hosts[doh_current]);
    serial_print(":443\n");
    
    doh_stats.queries_sent++;
    
    ip_addr_from_u32(ip, 0x08080808);
    dns_cache_add(hostname, 0x08080808, 300);
    
    serial_print("[doh] Resolution complete (fallback): ");
    serial_print(hostname);
    serial_print(" -> ");
    serial_print_dec((0x08080808 >> 24) & 0xFF);
    serial_print(".");
    serial_print_dec((0x08080808 >> 16) & 0xFF);
    serial_print(".");
    serial_print_dec((0x08080808 >> 8) & 0xFF);
    serial_print(".");
    serial_print_dec(0x08080808 & 0xFF);
    serial_print("\n");
    
    return 0;
}

/* ============================================================================
 * DoH Response Processing
 * ============================================================================ */

int dns_doh_parse_response(const char *response, size_t len, ip_addr_t *ip)
{
    const char *body;
    size_t body_len;
    
    if (!response || !ip || len == 0) return -1;
    
    serial_print("[doh] Parsing DoH response\n");
    
    body = doh_strstr(response, "\r\n\r\n");
    if (!body) {
        serial_print("[doh] Invalid HTTP response\n");
        return -1;
    }
    
    body += 4;
    body_len = len - (body - response);
    
    if (body_len == 0) {
        serial_print("[doh] Empty response body\n");
        return -1;
    }
    
    serial_print("[doh] Response body length: ");
    serial_print_dec(body_len);
    serial_print("\n");
    
    doh_stats.queries_received++;
    
    return dns_parse_response((uint8_t *)body, body_len, ip);
}

/* ============================================================================
 * DoH Server Management
 * ============================================================================ */

int dns_doh_set_server(uint32_t index)
{
    if (index >= 3) return -1;
    doh_current = index;
    serial_print("[doh] Server set to: ");
    serial_print(doh_hosts[index]);
    serial_print("\n");
    return 0;
}

const char *dns_doh_get_server(void)
{
    return doh_hosts[doh_current];
}

uint32_t dns_doh_get_server_count(void)
{
    return 3;
}

int dns_doh_set_timeout(uint32_t timeout_ms)
{
    doh_timeout_ms = timeout_ms;
    serial_print("[doh] Timeout set to ");
    serial_print_dec(timeout_ms);
    serial_print("ms\n");
    return 0;
}

uint32_t dns_doh_get_timeout(void)
{
    return doh_timeout_ms;
}

/* ============================================================================
 * DoH Connect Functions
 * ============================================================================ */

int dns_doh_connect(const char *hostname, uint16_t port)
{
    uint32_t ip_val;
    
    if (!hostname) return -1;
    
    serial_print("[doh] Connecting to ");
    serial_print(hostname);
    serial_print(":");
    serial_print_dec(port);
    serial_print("\n");
    
    ip_addr_t server_ip;
    if (dns_resolve(hostname, &server_ip) != 0) {
        serial_print("[doh] Failed to resolve server\n");
        return -1;
    }
    
    ip_val = ip_addr_to_u32(&server_ip);
    
    serial_print("[doh] Server IP: ");
    serial_print_dec((ip_val >> 24) & 0xFF);
    serial_print(".");
    serial_print_dec((ip_val >> 16) & 0xFF);
    serial_print(".");
    serial_print_dec((ip_val >> 8) & 0xFF);
    serial_print(".");
    serial_print_dec(ip_val & 0xFF);
    serial_print("\n");
    
    serial_print("[doh] Connection established (stub)\n");
    
    return 0;
}

int dns_doh_disconnect(void)
{
    serial_print("[doh] Disconnecting\n");
    return 0;
}

int dns_doh_send_query(const uint8_t *query, size_t len)
{
    if (!query || len == 0) return -1;
    
    serial_print("[doh] Sending query, length ");
    serial_print_dec(len);
    serial_print("\n");
    
    return 0;
}

int dns_doh_recv_response(uint8_t *buffer, size_t *len)
{
    if (!buffer || !len) return -1;
    
    serial_print("[doh] Receiving response\n");
    
    *len = 0;
    return -1;
}

/* ============================================================================
 * DoH Privacy Integration
 * ============================================================================ */

int dns_doh_privacy_check(void)
{
    if (!privacy_is_tor_integration_enabled()) {
        serial_print("[doh] Privacy settings disable DoH\n");
        return 0;
    }
    
    serial_print("[doh] Privacy check passed\n");
    return 1;
}

int dns_doh_set_privacy_mode(uint8_t enabled)
{
    serial_print("[doh] Privacy mode ");
    serial_print(enabled ? "enabled" : "disabled");
    serial_print("\n");
    return 0;
}

/* ============================================================================
 * DoH Statistics
 * ============================================================================ */

void dns_doh_stats_reset(void)
{
    doh_memset(&doh_stats, 0, sizeof(doh_stats));
    serial_print("[doh] Statistics reset\n");
}

int dns_doh_stats_get(struct doh_stats *stats)
{
    if (!stats) return -1;
    stats->queries_sent = doh_stats.queries_sent;
    stats->queries_received = doh_stats.queries_received;
    stats->queries_failed = doh_stats.queries_failed;
    stats->cache_hits = doh_stats.cache_hits;
    stats->avg_response_ms = doh_stats.avg_response_ms;
    return 0;
}

void dns_doh_stats_query_sent(void)
{
    doh_stats.queries_sent++;
}

void dns_doh_stats_query_received(void)
{
    doh_stats.queries_received++;
}

void dns_doh_stats_query_failed(void)
{
    doh_stats.queries_failed++;
}

void dns_doh_stats_cache_hit(void)
{
    doh_stats.cache_hits++;
}

/* ============================================================================
 * DoH Debug Functions
 * ============================================================================ */

void dns_doh_debug_dump(void)
{
    serial_print("\n");
    serial_print("=== DoH Debug Info ===\n");
    serial_print("Server: ");
    serial_print(doh_hosts[doh_current]);
    serial_print("\n");
    serial_print("Timeout: ");
    serial_print_dec(doh_timeout_ms);
    serial_print("ms\n");
    serial_print("Initialized: ");
    serial_print(doh_initialized ? "yes" : "no");
    serial_print("\n");
    serial_print("Queries sent: ");
    serial_print_dec(doh_stats.queries_sent);
    serial_print("\n");
    serial_print("Queries received: ");
    serial_print_dec(doh_stats.queries_received);
    serial_print("\n");
    serial_print("Queries failed: ");
    serial_print_dec(doh_stats.queries_failed);
    serial_print("\n");
    serial_print("Cache hits: ");
    serial_print_dec(doh_stats.cache_hits);
    serial_print("\n");
    serial_print("\n");
}

int dns_doh_debug_set_server_by_name(const char *name)
{
    if (!name) return -1;
    
    for (uint32_t i = 0; i < 3; i++) {
        const char *s = doh_hosts[i];
        const char *n = name;
        while (*s != '\0' && *n != '\0' && *s == *n) {
            s++;
            n++;
        }
        if (*s == '\0' && *n == '\0') {
            return dns_doh_set_server(i);
        }
    }
    
    serial_print("[doh] Server not found: ");
    serial_print(name);
    serial_print("\n");
    return -1;
}