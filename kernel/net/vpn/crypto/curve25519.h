/*
 * Lethal Kernel - Curve25519 / X25519 implementation
 *
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_VPN_CRYPTO_CURVE25519_H
#define LETHAL_NET_VPN_CRYPTO_CURVE25519_H

#include <stdint.h>

#define CURVE25519_KEY_SIZE   32
#define CURVE25519_SCALAR_SIZE 32


void curve25519_public_key(uint8_t pk[CURVE25519_KEY_SIZE],
                           const uint8_t sk[CURVE25519_SCALAR_SIZE]);

/*
 * ECDH X25519:
 *     shared = X25519(sk, peer_pk)
 */
int curve25519_shared(uint8_t shared[CURVE25519_KEY_SIZE],
                      const uint8_t sk[CURVE25519_SCALAR_SIZE],
                      const uint8_t peer_pk[CURVE25519_KEY_SIZE]);


void curve25519_scalarmult(uint8_t out[CURVE25519_KEY_SIZE],
                           const uint8_t scalar[CURVE25519_SCALAR_SIZE],
                           const uint8_t u[CURVE25519_KEY_SIZE]);


int curve25519_is_valid_public_key(const uint8_t pk[CURVE25519_KEY_SIZE]);


extern const uint8_t curve25519_basepoint[32];

#endif /* LETHAL_NET_VPN_CRYPTO_CURVE25519_H */