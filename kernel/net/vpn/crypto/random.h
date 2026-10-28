/*
 * Lethal Kernel - Cryptographically Secure Random Number Generator
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_VPN_CRYPTO_RANDOM_H
#define LETHAL_NET_VPN_CRYPTO_RANDOM_H

#include <stdint.h>
#include <stddef.h>


int random_init(void);


void random_add_entropy(const void *data, size_t len);


void random_bytes(void *buf, size_t len);


uint32_t random_u32(void);
uint64_t random_u64(void);


int random_has_rdrand(void);


int random_has_rdseed(void);

#endif /* LETHAL_NET_VPN_CRYPTO_RANDOM_H */