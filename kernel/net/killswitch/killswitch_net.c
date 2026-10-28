/*
 * Lethal Kernel - Kill Switch Network Integration
 *
 * v5.0: Network layer integration for kill switch.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */

#include "killswitch.h"
#include "../core/net.h"
#include "../core/device.h"
#include "../../core/console.h"

/* ============================================================================
 * Bring All Network Interfaces Down
 * ============================================================================ */

int killswitch_net_interface_down_all(void)
{
    int count;
    int i;

    serial_print("[killswitch] Taking down all network interfaces\n");

    count = net_device_count();

    for (i = 0; i < count; i++) {
        struct net_device *dev;

        dev = net_device_get(i);

        if (!dev)
            continue;

        /*
         * struct net_device does not contain an "initialized" member.
         *
         * Use the device flags to determine whether the interface is
         * currently active.
         */
        if ((dev->flags & NET_DEV_FLAG_UP) == 0)
            continue;

        serial_print("[killswitch] Taking down interface ");

        serial_print_dec((uint32_t)i);

        serial_print("\n");

        /*
         * Clear both administrative UP and operational RUNNING state.
         */
        dev->flags &= ~NET_DEV_FLAG_UP;
        dev->flags &= ~NET_DEV_FLAG_RUNNING;
    }

    return 0;
}

/* ============================================================================
 * Bring All Network Interfaces Up
 * ============================================================================ */

int killswitch_net_interface_up_all(void)
{
    int count;
    int i;

    serial_print("[killswitch] Bringing up all network interfaces\n");

    count = net_device_count();

    for (i = 0; i < count; i++) {
        struct net_device *dev;

        dev = net_device_get(i);

        if (!dev)
            continue;

        /*
         * An interface that is already UP does not need to be changed.
         */
        if ((dev->flags & NET_DEV_FLAG_UP) != 0)
            continue;

        serial_print("[killswitch] Bringing up interface ");

        serial_print_dec((uint32_t)i);

        serial_print("\n");

        /*
         * Restore the interface flags.
         */
        dev->flags |= NET_DEV_FLAG_UP;
        dev->flags |= NET_DEV_FLAG_RUNNING;
    }

    return 0;
}

/* ============================================================================
 * VPN Status
 * ============================================================================ */

void killswitch_check_vpn_status(void)
{
    /*
     * In a complete implementation this function should check the state
     * of the VPN/tunnel interface.
     *
     * For now we only log the check.
     */
    serial_print("[killswitch] Checking VPN status\n");
}

/* ============================================================================
 * DNS Protection
 * ============================================================================ */

int killswitch_dns_protection_enabled(void)
{
    /*
     * When the kill switch is enabled, DNS leak protection is enabled too.
     */
    return killswitch_is_enabled();
}
