/*
 * Lethal Kernel - DNS over HTTPS Header
 *
 * v5.0: DNS over HTTPS for encrypted DNS queries.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_DNS_DOH_H
#define LETHAL_NET_DNS_DOH_H

#include <stdint.h>
#include <stddef.h>
#include "../core/net.h"

/* DoH statistics structure */
struct doh_stats {
    uint32_t queries_sent;
    uint32_t queries_received;
    uint32_t queries_failed;
    uint32_t cache_hits;
    uint32_t avg_response_ms;
};

/* Initialization */
int dns_doh_init(void);
int dns_doh_shutdown(void);

/* Resolution */
int dns_doh_resolve(const char *hostname, ip_addr_t *ip);
int dns_doh_parse_response(const char *response, size_t len, ip_addr_t *ip);

/* Server management */
int dns_doh_set_server(uint32_t index);
const char *dns_doh_get_server(void);
uint32_t dns_doh_get_server_count(void);
int dns_doh_set_timeout(uint32_t timeout_ms);
uint32_t dns_doh_get_timeout(void);

/* Connection */
int dns_doh_connect(const char *hostname, uint16_t port);
int dns_doh_disconnect(void);
int dns_doh_send_query(const uint8_t *query, size_t len);
int dns_doh_recv_response(uint8_t *buffer, size_t *len);

/* Privacy */
int dns_doh_privacy_check(void);
int dns_doh_set_privacy_mode(uint8_t enabled);

/* Statistics */
void dns_doh_stats_reset(void);
int dns_doh_stats_get(struct doh_stats *stats);
void dns_doh_stats_query_sent(void);
void dns_doh_stats_query_received(void);
void dns_doh_stats_query_failed(void);
void dns_doh_stats_cache_hit(void);

/* Debug */
void dns_doh_debug_dump(void);
int dns_doh_debug_set_server_by_name(const char *name);

#endif /* LETHAL_NET_DNS_DOH_H */