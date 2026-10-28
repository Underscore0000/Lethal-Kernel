/*
 * Lethal Kernel - DNS Header
 *
 * v5.0: DNS resolution with caching and DoH support.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_DNS_DNS_H
#define LETHAL_NET_DNS_DNS_H

#include <stdint.h>
#include <stddef.h>
#include "../core/net.h"
#include <net/socket.h>

/* DNS configuration */
struct dns_config {
    char primary_dns[64];
    char secondary_dns[64];
    uint32_t timeout_ms;
    uint8_t retries;
    uint8_t use_doh;
    char doh_server[256];
    uint32_t cache_ttl;
};

/* DNS header structures */
struct dns_header {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed));

struct dns_question {
    uint16_t qtype;
    uint16_t qclass;
} __attribute__((packed));

struct dns_answer {
    uint16_t name;
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t rdlength;
    uint8_t rdata[0];
} __attribute__((packed));

/* Initialization */
int dns_init(void);
int dns_shutdown(void);

/* Resolution */
int dns_resolve(const char *hostname, ip_addr_t *addr);
int dns_resolve_doh(const char *hostname, ip_addr_t *addr);

/* Configuration */
int dns_set_config(struct dns_config *config);
int dns_get_config(struct dns_config *config);
int dns_set_server(const char *primary, const char *secondary);
int dns_set_doh(const char *server, uint8_t enabled);

/* Utility functions */
int dns_build_query(const char *hostname, uint8_t *buffer, size_t *len);
int dns_parse_response(uint8_t *data, size_t len, ip_addr_t *ip);

#endif /* LETHAL_NET_DNS_DNS_H */