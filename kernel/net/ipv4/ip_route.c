/*
 * Lethal Kernel - IPv4 Routing Table Implementation
 *
 * v5.0: Real routing table with longest prefix match.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */

#include "ip_route.h"
#include "ip.h"

#include "../../core/console.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define ROUTE_LOCAL_NETMASK 0xFFFFFF00U

/* ============================================================================
 * Routing Table
 * ============================================================================ */

static struct route_entry route_table[ROUTE_TABLE_SIZE];
static uint32_t route_count = 0;

/* ============================================================================
 * Helpers
 * ============================================================================ */

/*
 * Count the number of one bits in a netmask.
 *
 * Routing masks are expected to be contiguous, e.g.:
 *
 *   255.255.255.0 -> 24
 *   255.255.0.0   -> 16
 *   255.0.0.0     -> 8
 *   0.0.0.0       -> 0
 */
static uint32_t route_prefix_length(uint32_t netmask)
{
    uint32_t prefix = 0;

    while (netmask != 0U) {
        prefix += netmask & 1U;
        netmask >>= 1;
    }

    return prefix;
}

/*
 * Print an IPv4 address represented as a uint32_t.
 *
 * Addresses used by the routing layer are represented consistently with
 * ip_addr_to_u32().
 */
static void route_print_ip(uint32_t ip)
{
    serial_print_dec((ip >> 24) & 0xFFU);
    serial_print(".");
    serial_print_dec((ip >> 16) & 0xFFU);
    serial_print(".");
    serial_print_dec((ip >> 8) & 0xFFU);
    serial_print(".");
    serial_print_dec(ip & 0xFFU);
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

int ip_route_init(void)
{
    ip_addr_t local;
    uint32_t local_ip;
    uint32_t network;
    int result;

    serial_print("[ip_route] Initializing routing table...\n");

    /*
     * Clear the entire routing table.
     */
    memset(route_table, 0, sizeof(route_table));
    route_count = 0;

    /*
     * Get the configured local IPv4 address from the IP layer.
     *
     * We deliberately do NOT access fields such as:
     *
     *     dev->ip
     *     dev->netmask
     *     dev->gateway
     *
     * because those fields are not part of struct net_device in this kernel.
     */
    memset(&local, 0, sizeof(local));
    ip_get_local(&local);

    local_ip = ip_addr_to_u32(&local);

    /*
     * Add the directly-connected local /24 network.
     *
     * The current IP configuration uses a /24 LAN. Gateway discovery can
     * be added later when the device/network configuration exposes that
     * information.
     */
    network = local_ip & ROUTE_LOCAL_NETMASK;

    result = ip_route_add(network,
                          ROUTE_LOCAL_NETMASK,
                          0,
                          0);

    if (result != 0) {
        serial_print("[ip_route] Failed to add local network route\n");
        return -1;
    }

    serial_print("[ip_route] Local network: ");
    route_print_ip(network);
    serial_print("/24\n");

    serial_print("[ip_route] Routing table initialized\n");

    return 0;
}

/* ============================================================================
 * Route Management
 * ============================================================================ */

int ip_route_add(uint32_t dest,
                 uint32_t netmask,
                 uint32_t gateway,
                 uint32_t dev_id)
{
    uint32_t i;

    /*
     * If an identical destination/mask already exists, update it instead
     * of allocating another table entry.
     */
    for (i = 0; i < route_count; i++) {
        if (!route_table[i].valid)
            continue;

        if (route_table[i].dest == dest &&
            route_table[i].netmask == netmask) {

            route_table[i].gateway = gateway;
            route_table[i].dev_id = dev_id;

            return 0;
        }
    }

    if (route_count >= ROUTE_TABLE_SIZE) {
        serial_print("[ip_route] Route table full\n");
        return -1;
    }

    route_table[route_count].dest = dest;
    route_table[route_count].netmask = netmask;
    route_table[route_count].gateway = gateway;
    route_table[route_count].dev_id = dev_id;
    route_table[route_count].metric = 0;
    route_table[route_count].refcount = 0;
    route_table[route_count].valid = 1;

    route_count++;

    serial_print("[ip_route] Added route\n");

    return 0;
}

/* ============================================================================
 * Route Delete
 * ============================================================================ */

int ip_route_del(uint32_t dest, uint32_t netmask)
{
    uint32_t i;

    for (i = 0; i < route_count; i++) {
        if (!route_table[i].valid)
            continue;

        if (route_table[i].dest == dest &&
            route_table[i].netmask == netmask) {

            route_table[i].valid = 0;

            return 0;
        }
    }

    return -1;
}

/* ============================================================================
 * Route Lookup - Longest Prefix Match
 * ============================================================================ */

int ip_route_lookup(uint32_t dest,
                    uint32_t *next_hop,
                    uint32_t *dev_id)
{
    int best_idx = -1;
    uint32_t best_prefix = 0;
    uint32_t i;

    if (!next_hop || !dev_id)
        return -1;

    for (i = 0; i < route_count; i++) {
        uint32_t netmask;
        uint32_t prefix;

        if (!route_table[i].valid)
            continue;

        netmask = route_table[i].netmask;

        /*
         * Check whether destination belongs to this network.
         */
        if ((dest & netmask) !=
            (route_table[i].dest & netmask)) {
            continue;
        }

        prefix = route_prefix_length(netmask);

        /*
         * Longest-prefix match.
         */
        if (best_idx < 0 || prefix > best_prefix) {
            best_prefix = prefix;
            best_idx = (int)i;
        }
    }

    if (best_idx < 0) {
        serial_print("[ip_route] No route to destination\n");
        return -1;
    }

    {
        struct route_entry *route;

        route = &route_table[best_idx];

        route->refcount++;

        /*
         * Gateway route:
         *
         *     destination -> gateway
         *
         * Direct route:
         *
         *     destination -> destination
         */
        if (route->gateway != 0U) {
            *next_hop = route->gateway;
        } else {
            *next_hop = dest;
        }

        *dev_id = route->dev_id;
    }

    return 0;
}

/* ============================================================================
 * Route Dump
 * ============================================================================ */

void ip_route_dump(void)
{
    uint32_t i;

    serial_print("[ip_route] Routing table:\n");

    for (i = 0; i < route_count; i++) {
        uint32_t dest;
        uint32_t netmask;
        uint32_t gateway;
        uint32_t prefix;

        if (!route_table[i].valid)
            continue;

        dest = route_table[i].dest;
        netmask = route_table[i].netmask;
        gateway = route_table[i].gateway;
        prefix = route_prefix_length(netmask);

        serial_print("  ");

        route_print_ip(dest);

        serial_print("/");

        serial_print_dec(prefix);

        serial_print(" -> ");

        if (gateway != 0U) {
            serial_print("gw ");
            route_print_ip(gateway);
        } else {
            serial_print("direct");
        }

        serial_print(" dev ");
        serial_print_dec(route_table[i].dev_id);

        serial_print("\n");
    }
}
