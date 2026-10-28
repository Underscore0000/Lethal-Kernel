/*
 * Lethal Kernel - TCP Input Functions
 *
 * v5.0: TCP packet reception.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "tcp.h"
#include "../core/net.h"
#include "../core/sk_buff.h"
#include "../../core/console.h"

/* Forward declaration di tcp_process_packet da tcp.c */
extern int tcp_process_packet(struct sk_buff *skb);

int tcp_receive(struct sk_buff *skb)
{
    if (!skb) return -1;
    
    serial_print("[tcp_input] Received packet, len ");
    serial_print_dec(skb->len);
    serial_print("\n");
    
    /* Passa il pacchetto a tcp_process_packet */
    return tcp_process_packet(skb);
}