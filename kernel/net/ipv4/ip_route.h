/*
 * Lethal Kernel - IPv4 Routing Table
 *
 * v5.0: Routing table management for IPv4.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef LETHAL_NET_IPV4_IP_ROUTE_H
#define LETHAL_NET_IPV4_IP_ROUTE_H

#include <stdint.h>

#include "../core/net.h"

/* ============================================================================
 * Routing Table Constants
 * ============================================================================ */

#define ROUTE_TABLE_SIZE 32

/* ============================================================================
 * Route Flags
 * ============================================================================ */

#define ROUTE_FLAG_UP       (1U << 0)
#define ROUTE_FLAG_GATEWAY  (1U << 1)
#define ROUTE_FLAG_HOST     (1U << 2)

/* ============================================================================
 * Routing Table Entry
 * ============================================================================ */

struct route_entry {
    uint32_t dest;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dev_id;

    uint8_t valid;

    uint32_t metric;
    uint32_t refcount;
};

/* ============================================================================
 * Routing API
 * ============================================================================ */

int ip_route_init(void);

int ip_route_add(uint32_t dest,
                 uint32_t netmask,
                 uint32_t gateway,
                 uint32_t dev_id);

int ip_route_del(uint32_t dest,
                 uint32_t netmask);

int ip_route_lookup(uint32_t dest,
                    uint32_t *next_hop,
                    uint32_t *dev_id);

void ip_route_dump(void);

#endif /* LETHAL_NET_IPV4_IP_ROUTE_H */
