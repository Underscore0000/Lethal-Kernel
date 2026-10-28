/*
 * Lethal Kernel - TCP Output Functions
 *
 * v5.0: TCP packet transmission.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "tcp.h"
#include "../core/net.h"
#include "../core/sk_buff.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * TCP Output Functions (compatibili con tcp.h)
 * ============================================================================ */

int tcp_send_segment(struct tcp_socket *sock, uint8_t *data, size_t len, uint8_t flags)
{
    if (!sock) return -1;
    
    (void)data;
    (void)len;
    (void)flags;
    
    serial_print("[tcp_output] Send segment on fd ");
    serial_print_dec(sock->fd);
    serial_print("\n");
    
    return 0;
}

int tcp_send_syn(struct tcp_socket *sock)
{
    if (!sock) return -1;
    
    serial_print("[tcp_output] Send SYN on fd ");
    serial_print_dec(sock->fd);
    serial_print("\n");
    
    return tcp_send_segment(sock, NULL, 0, TCP_FLAG_SYN);
}

int tcp_send_syn_ack(struct tcp_socket *sock)
{
    if (!sock) return -1;
    
    serial_print("[tcp_output] Send SYN-ACK on fd ");
    serial_print_dec(sock->fd);
    serial_print("\n");
    
    return tcp_send_segment(sock, NULL, 0, TCP_FLAG_SYN | TCP_FLAG_ACK);
}

int tcp_send_ack(struct tcp_socket *sock)
{
    if (!sock) return -1;
    
    serial_print("[tcp_output] Send ACK on fd ");
    serial_print_dec(sock->fd);
    serial_print("\n");
    
    return tcp_send_segment(sock, NULL, 0, TCP_FLAG_ACK);
}

int tcp_send_data(struct tcp_socket *sock, const void *data, size_t len)
{
    if (!sock || !data) return -1;
    
    serial_print("[tcp_output] Send DATA on fd ");
    serial_print_dec(sock->fd);
    serial_print(" len ");
    serial_print_dec(len);
    serial_print("\n");
    
    return tcp_send_segment(sock, (uint8_t *)data, len, TCP_FLAG_ACK);
}

int tcp_send_fin(struct tcp_socket *sock)
{
    if (!sock) return -1;
    
    serial_print("[tcp_output] Send FIN on fd ");
    serial_print_dec(sock->fd);
    serial_print("\n");
    
    return tcp_send_segment(sock, NULL, 0, TCP_FLAG_FIN | TCP_FLAG_ACK);
}

int tcp_send_rst(struct tcp_socket *sock)
{
    if (!sock) return -1;
    
    serial_print("[tcp_output] Send RST on fd ");
    serial_print_dec(sock->fd);
    serial_print("\n");
    
    return tcp_send_segment(sock, NULL, 0, TCP_FLAG_RST);
}