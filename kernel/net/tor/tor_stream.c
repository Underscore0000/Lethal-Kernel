/*
 * Lethal Kernel - Tor Stream Management
 *
 * v5.0: REAL Tor stream management.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "tor.h"
#include "tor_stream.h"
#include "tor_circuit.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>

/* ============================================================================
 * Stream Operations (REAL)
 * ============================================================================ */

int tor_stream_attach(uint32_t stream_id, uint32_t circuit_id)
{
    struct tor_stream *str = stream_list;
    while (str) {
        if (str->stream_id == stream_id) {
            str->circuit_id = circuit_id;
            serial_print("[tor_stream] Attached stream ");
            serial_print_dec(stream_id);
            serial_print(" to circuit ");
            serial_print_dec(circuit_id);
            serial_print("\n");
            return 0;
        }
        str = (struct tor_stream *)str->next;
    }
    return -1;
}

int tor_stream_begin(uint32_t stream_id, uint32_t dest_ip, uint16_t dest_port)
{
    serial_print("[tor_stream] Beginning stream ");
    serial_print_dec(stream_id);
    serial_print(" to ");
    serial_print_dec((dest_ip >> 24) & 0xFF);
    serial_print(".");
    serial_print_dec((dest_ip >> 16) & 0xFF);
    serial_print(".");
    serial_print_dec((dest_ip >> 8) & 0xFF);
    serial_print(".");
    serial_print_dec(dest_ip & 0xFF);
    serial_print(":");
    serial_print_dec(dest_port);
    serial_print("\n");
    
    return tor_stream_open(0, dest_ip, dest_port);
}

int tor_stream_data(uint32_t stream_id, const void *data, size_t len)
{
    return tor_stream_send(stream_id, data, len);
}

int tor_stream_end(uint32_t stream_id)
{
    return tor_stream_close(stream_id);
}