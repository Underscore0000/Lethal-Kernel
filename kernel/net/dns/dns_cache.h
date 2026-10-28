/*
 * Lethal Kernel - DNS Cache Header
 *
 * v5.0: DNS caching with TTL support.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_DNS_DNS_CACHE_H
#define LETHAL_NET_DNS_DNS_CACHE_H

#include <stdint.h>
#include <stddef.h>


void dns_cache_init(void);
int dns_cache_add(const char *name, uint32_t ip, uint32_t ttl);
int dns_cache_lookup(const char *name, uint32_t *ip);
int dns_cache_clear(void);
int dns_cache_flush(void);


int dns_cache_get_size(void);
int dns_cache_get_max_size(void);
int dns_cache_remove(const char *name);
void dns_cache_dump(void);

#endif /* LETHAL_NET_DNS_DNS_CACHE_H */