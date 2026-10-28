/*
 * Lethal Kernel - TCP Timers
 *
 * v5.0: TCP retransmission timers and timeouts.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_IPV4_TCP_TIMER_H
#define LETHAL_NET_IPV4_TCP_TIMER_H

#include <stdint.h>
#include "tcp.h"

/* TCP timer states */
#define TCP_TIMER_INACTIVE  0
#define TCP_TIMER_RUNNING   1
#define TCP_TIMER_EXPIRED   2

/* TCP timer structure */
struct tcp_timer {
    uint32_t timeout_ms;
    uint32_t start_time;
    uint8_t state;
    uint32_t retransmit_count;
    uint32_t max_retransmit;
    void *callback_data;
};

/* Function prototypes */
void tcp_timer_init(void);
void tcp_timer_start(struct tcp_socket *sock, uint32_t timeout_ms);
void tcp_timer_stop(struct tcp_socket *sock);
void tcp_timer_tick(void);
void tcp_timer_handle_retransmit(struct tcp_socket *sock);

#endif /* LETHAL_NET_IPV4_TCP_TIMER_H */