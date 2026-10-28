/*
 * Lethal Kernel - Network Driver Interface
 *
 * v5.0: Common interface for all network drivers.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_DRIVERS_NET_DRIVER_H
#define LETHAL_NET_DRIVERS_NET_DRIVER_H

#include <stdint.h>
#include "../core/net.h"
#include "../core/sk_buff.h"

/* Driver flags */
#define NET_DRIVER_FLAG_UP          (1 << 0)
#define NET_DRIVER_FLAG_RUNNING     (1 << 1)
#define NET_DRIVER_FLAG_PROMISC     (1 << 2)

/* Driver statistics */
struct net_driver_stats {
    uint64_t rx_packets;
    uint64_t rx_bytes;
    uint64_t rx_errors;
    uint64_t rx_dropped;
    uint64_t tx_packets;
    uint64_t tx_bytes;
    uint64_t tx_errors;
    uint64_t tx_dropped;
    uint64_t interrupts;
};

/* Network driver operations */
struct net_driver_ops {
    int (*init)(void *driver_data);
    int (*open)(void *driver_data);
    int (*close)(void *driver_data);
    int (*send)(void *driver_data, struct sk_buff *skb);
    int (*receive)(void *driver_data, struct sk_buff **skb);
    int (*ioctl)(void *driver_data, uint32_t cmd, void *arg);
    void (*irq_handler)(void *driver_data);
};

/* Network driver structure */
struct net_driver {
    char name[32];
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t flags;
    uint32_t mtu;
    mac_addr_t mac;
    void *driver_data;
    struct net_driver_ops ops;
    struct net_driver_stats stats;
    uint8_t initialized;
};

/* Function prototypes */
int net_driver_register(struct net_driver *drv);
int net_driver_unregister(struct net_driver *drv);
struct net_driver *net_driver_find(uint32_t vendor_id, uint32_t device_id);
void net_driver_irq_handler(uint32_t irq, void *data);

#endif /* LETHAL_NET_DRIVERS_NET_DRIVER_H */