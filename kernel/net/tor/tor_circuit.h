/*
 * Lethal Kernel - Tor Circuit Management Header
 *
 * v5.0: Tor circuit management functions.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_TOR_TOR_CIRCUIT_H
#define LETHAL_NET_TOR_TOR_CIRCUIT_H

#include <stdint.h>
#include "tor.h"

/* Circuit operations */
int tor_circuit_build(uint32_t circuit_id);
int tor_circuit_extend(uint32_t circuit_id, uint32_t node_ip);
int tor_circuit_close(uint32_t circuit_id);

/* Circuit status */
uint32_t tor_circuit_get_used(uint32_t circuit_id);
int tor_circuit_rotate(void);

/* External variables (dichiarate in tor.c) */
extern struct tor_circuit *circuit_list;

#endif /* LETHAL_NET_TOR_TOR_CIRCUIT_H */