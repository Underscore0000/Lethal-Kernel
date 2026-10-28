/*
 * Lethal Kernel - Tor Control Interface Header
 *
 * v5.0: Control interface for Tor operations.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_TOR_TOR_CONTROL_H
#define LETHAL_NET_TOR_TOR_CONTROL_H

#include <stdint.h>
#include <stddef.h>
#include "tor.h"

/* Control commands */
int tor_control_start(void);
int tor_control_stop(void);
int tor_control_restart(void);
int tor_control_status(void);

/* Control configuration */
int tor_control_set_enabled(uint8_t enabled);
int tor_control_set_auto_start(uint8_t auto_start);
int tor_control_set_timeout(uint32_t timeout_ms);
int tor_control_set_max_circuits(uint8_t max_circuits);

/* Control utility functions */
int tor_control_new_circuit(void);
int tor_control_close_circuit(uint32_t circuit_id);
int tor_control_list_circuits(void);
int tor_control_list_streams(void);

/* Control file interface */
int tor_control_load_config(const char *path);
int tor_control_save_config(const char *path);

#endif /* LETHAL_NET_TOR_TOR_CONTROL_H */