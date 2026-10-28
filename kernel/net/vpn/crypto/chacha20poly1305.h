/*
 * Lethal Kernel - ChaCha20-Poly1305 AEAD (RFC 8439)
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_NET_VPN_CRYPTO_CHACHA20POLY1305_H
#define LETHAL_NET_VPN_CRYPTO_CHACHA20POLY1305_H

#include <stdint.h>
#include <stddef.h>

#define CHACHA20_KEY_SIZE     32
#define CHACHA20_NONCE_SIZE   12
#define POLY1305_KEY_SIZE     32
#define POLY1305_TAG_SIZE     16
#define AEAD_TAG_SIZE         POLY1305_TAG_SIZE


void chacha20_xor(uint8_t *out, const uint8_t *in, size_t len,
                  const uint8_t key[32], const uint8_t nonce[12],
                  uint32_t counter);


void poly1305_mac(uint8_t tag[16], const uint8_t *msg, size_t len,
                  const uint8_t key[32]);


int chacha20poly1305_encrypt(uint8_t *ct, uint8_t tag[16],
                             const uint8_t *pt, size_t pt_len,
                             const uint8_t *aad, size_t aad_len,
                             const uint8_t nonce[12],
                             const uint8_t key[32]);

int chacha20poly1305_decrypt(uint8_t *pt,
                             const uint8_t *ct, size_t ct_len,
                             const uint8_t *aad, size_t aad_len,
                             const uint8_t tag[16],
                             const uint8_t nonce[12],
                             const uint8_t key[32]);


void chacha20poly1305_derive_poly_key(uint8_t poly_key[32],
                                      const uint8_t nonce[12],
                                      const uint8_t key[32]);

#endif /* LETHAL_NET_VPN_CRYPTO_CHACHA20POLY1305_H */