/*
 * Lethal Kernel - Tor Stream Management Header
 *
 * v5.0: Tor stream management functions.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_TOR_TOR_STREAM_H
#define LETHAL_NET_TOR_TOR_STREAM_H

#include <stdint.h>
#include <stddef.h>
#include "tor.h"

/* Stream operations */
int tor_stream_attach(uint32_t stream_id, uint32_t circuit_id);
int tor_stream_begin(uint32_t stream_id, uint32_t dest_ip, uint16_t dest_port);
int tor_stream_data(uint32_t stream_id, const void *data, size_t len);
int tor_stream_end(uint32_t stream_id);

/* External variables (dichiarate in tor.c) */
extern struct tor_stream *stream_list;

#endif /* LETHAL_NET_TOR_TOR_STREAM_H */