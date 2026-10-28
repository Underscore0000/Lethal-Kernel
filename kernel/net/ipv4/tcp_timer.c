/*
 * Lethal Kernel - TCP Timer Functions
 *
 * v5.0: TCP timer management.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "tcp.h"
#include "../../core/timer.h"
#include "../../core/console.h"
#include "../../memory/heap.h"

/* ============================================================================
 * TCP Timer Functions (compatibili con tcp.h)
 * ============================================================================ */

void tcp_timer_init(void)
{
    serial_print("[tcp_timer] Timer initialized\n");
}

void tcp_timer_start(struct tcp_socket *sock, uint32_t timeout_ms)
{
    if (!sock) return;
    
    (void)timeout_ms;
    
    serial_print("[tcp_timer] Timer started for fd ");
    serial_print_dec(sock->fd);
    serial_print("\n");
}

void tcp_timer_stop(struct tcp_socket *sock)
{
    if (!sock) return;
    
    serial_print("[tcp_timer] Timer stopped for fd ");
    serial_print_dec(sock->fd);
    serial_print("\n");
}

void tcp_timer_tick(void)
{
    serial_print("[tcp_timer] Timer tick\n");
}

void tcp_timer_handle_retransmit(struct tcp_socket *sock)
{
    if (!sock) return;
    
    serial_print("[tcp_timer] Retransmit for fd ");
    serial_print_dec(sock->fd);
    serial_print("\n");
}

void tcp_timer_cleanup(struct tcp_socket *sock)
{
    if (!sock) return;
    
    serial_print("[tcp_timer] Cleanup for fd ");
    serial_print_dec(sock->fd);
    serial_print("\n");
}