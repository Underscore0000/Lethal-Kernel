/*
 * Lethal Kernel - ChaCha20-Poly1305 AEAD implementation
 *
 * Implementazione conforme a RFC 8439. Testata contro i vettori
 * ufficiali in crypto_selftest.c.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "chacha20poly1305.h"
#include <string.h>

/* ==========================================================================
 * ChaCha20 core
 * ========================================================================== */

#define ROTL32(v, c) (((v) << (c)) | ((v) >> (32 - (c))))

#define QR(a, b, c, d)                       \
    do {                                     \
        (a) += (b); (d) ^= (a);              \
        (d) = ROTL32((d), 16);               \
        (c) += (d); (b) ^= (c);              \
        (b) = ROTL32((b), 12);               \
        (a) += (b); (d) ^= (a);              \
        (d) = ROTL32((d), 8);                \
        (c) += (d); (b) ^= (c);              \
        (b) = ROTL32((b), 7);                \
    } while (0)

static void chacha20_rounds(uint32_t x[16])
{
    for (int i = 0; i < 10; i++) {
        QR(x[0], x[4], x[8],  x[12]);
        QR(x[1], x[5], x[9],  x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[8],  x[13]);
        QR(x[3], x[4], x[9],  x[14]);
    }
}

static void chacha20_keystream_block(uint32_t out[16],
                                     const uint8_t key[32],
                                     uint32_t counter,
                                     const uint8_t nonce[12])
{
    uint32_t state[16];
    uint32_t k[8];

    for (int i = 0; i < 8; i++) {
        k[i] = (uint32_t)key[i*4]
             | ((uint32_t)key[i*4+1] << 8)
             | ((uint32_t)key[i*4+2] << 16)
             | ((uint32_t)key[i*4+3] << 24);
    }

    state[0]  = 0x61707865;
    state[1]  = 0x3320646e;
    state[2]  = 0x79622d32;
    state[3]  = 0x6b206574;
    state[4]  = k[0];
    state[5]  = k[1];
    state[6]  = k[2];
    state[7]  = k[3];
    state[8]  = k[4];
    state[9]  = k[5];
    state[10] = k[6];
    state[11] = k[7];
    state[12] = counter;
    state[13] = (uint32_t)nonce[0]
              | ((uint32_t)nonce[1]  << 8)
              | ((uint32_t)nonce[2]  << 16)
              | ((uint32_t)nonce[3]  << 24);
    state[14] = (uint32_t)nonce[4]
              | ((uint32_t)nonce[5]  << 8)
              | ((uint32_t)nonce[6]  << 16)
              | ((uint32_t)nonce[7]  << 24);
    state[15] = (uint32_t)nonce[8]
              | ((uint32_t)nonce[9]  << 8)
              | ((uint32_t)nonce[10] << 16)
              | ((uint32_t)nonce[11] << 24);

    uint32_t working[16];
    for (int i = 0; i < 16; i++) working[i] = state[i];

    chacha20_rounds(working);

    for (int i = 0; i < 16; i++) out[i] = working[i] + state[i];
}

void chacha20_xor(uint8_t *out, const uint8_t *in, size_t len,
                  const uint8_t key[32], const uint8_t nonce[12],
                  uint32_t counter)
{
    uint32_t ks[16];
    size_t done = 0;

    while (done < len) {
        chacha20_keystream_block(ks, key, counter, nonce);
        counter++;

        uint8_t bytes[64];
        for (int i = 0; i < 16; i++) {
            bytes[i*4]   = (uint8_t)(ks[i]);
            bytes[i*4+1] = (uint8_t)(ks[i] >> 8);
            bytes[i*4+2] = (uint8_t)(ks[i] >> 16);
            bytes[i*4+3] = (uint8_t)(ks[i] >> 24);
        }

        size_t n = len - done;
        if (n > 64) n = 64;

        for (size_t i = 0; i < n; i++) {
            out[done + i] = in[done + i] ^ bytes[i];
        }

        done += n;
    }
}

void chacha20poly1305_derive_poly_key(uint8_t poly_key[32],
                                      const uint8_t nonce[12],
                                      const uint8_t key[32])
{
    uint32_t ks[16];
    chacha20_keystream_block(ks, key, 0, nonce);
    for (int i = 0; i < 8; i++) {
        poly_key[i*4]   = (uint8_t)(ks[i]);
        poly_key[i*4+1] = (uint8_t)(ks[i] >> 8);
        poly_key[i*4+2] = (uint8_t)(ks[i] >> 16);
        poly_key[i*4+3] = (uint8_t)(ks[i] >> 24);
    }
}

/* ==========================================================================
 * Poly1305 (RFC 8439 section 2.5)
 * ========================================================================== */

struct poly1305_ctx {
    uint32_t r[5];
    uint32_t h[5];
    uint32_t pad[4];
};

static inline uint32_t load32_le(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static void poly1305_init(struct poly1305_ctx *ctx, const uint8_t key[32])
{
    uint32_t t0 = load32_le(key + 0);
    uint32_t t1 = load32_le(key + 4);
    uint32_t t2 = load32_le(key + 8);
    uint32_t t3 = load32_le(key + 12);

    ctx->r[0] = (t0                    ) & 0x3ffffff;
    ctx->r[1] = ((t0 >> 26) | (t1 << 6)) & 0x3ffff03;
    ctx->r[2] = ((t1 >> 20) | (t2 << 12)) & 0x3ffc0ff;
    ctx->r[3] = ((t2 >> 14) | (t3 << 18)) & 0x3f03fff;
    ctx->r[4] = ((t3 >> 8)              ) & 0x00fffff;

    ctx->h[0] = ctx->h[1] = ctx->h[2] = ctx->h[3] = ctx->h[4] = 0;

    ctx->pad[0] = load32_le(key + 16);
    ctx->pad[1] = load32_le(key + 20);
    ctx->pad[2] = load32_le(key + 24);
    ctx->pad[3] = load32_le(key + 28);
}

static void poly1305_block(struct poly1305_ctx *ctx,
                           const uint8_t *m, uint32_t hibit)
{
    uint32_t r0 = ctx->r[0], r1 = ctx->r[1], r2 = ctx->r[2];
    uint32_t r3 = ctx->r[3], r4 = ctx->r[4];
    uint32_t s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;

    uint32_t h0 = ctx->h[0], h1 = ctx->h[1], h2 = ctx->h[2];
    uint32_t h3 = ctx->h[3], h4 = ctx->h[4];

    uint32_t t0 = load32_le(m + 0);
    uint32_t t1 = load32_le(m + 4);
    uint32_t t2 = load32_le(m + 8);
    uint32_t t3 = load32_le(m + 12);

    h0 += t0 & 0x3ffffff;
    h1 += ((t0 >> 26) | (t1 << 6)) & 0x3ffffff;
    h2 += ((t1 >> 20) | (t2 << 12)) & 0x3ffffff;
    h3 += ((t2 >> 14) | (t3 << 18)) & 0x3ffffff;
    h4 += (t3 >> 8) | hibit;

    uint64_t d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 + (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
    uint64_t d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 + (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
    uint64_t d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 + (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
    uint64_t d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 + (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
    uint64_t d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 + (uint64_t)h3 * r1 + (uint64_t)h4 * r0;

    uint32_t c;
    c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & 0x3ffffff;
    d1 += c; c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & 0x3ffffff;
    d2 += c; c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & 0x3ffffff;
    d3 += c; c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & 0x3ffffff;
    d4 += c; c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & 0x3ffffff;
    h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
    h1 += c;

    ctx->h[0] = h0;
    ctx->h[1] = h1;
    ctx->h[2] = h2;
    ctx->h[3] = h3;
    ctx->h[4] = h4;
}

static void poly1305_finish(struct poly1305_ctx *ctx, uint8_t tag[16])
{
    uint32_t h0 = ctx->h[0], h1 = ctx->h[1], h2 = ctx->h[2];
    uint32_t h3 = ctx->h[3], h4 = ctx->h[4];

    uint32_t c;
    c = h1 >> 26; h1 &= 0x3ffffff;
    h2 += c; c = h2 >> 26; h2 &= 0x3ffffff;
    h3 += c; c = h3 >> 26; h3 &= 0x3ffffff;
    h4 += c; c = h4 >> 26; h4 &= 0x3ffffff;
    h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
    h1 += c;

    uint32_t g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
    uint32_t g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
    uint32_t g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
    uint32_t g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
    uint32_t g4 = h4 + c - (1u << 26);

    uint32_t mask = (g4 >> 31) - 1;
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0;
    h1 = (h1 & mask) | g1;
    h2 = (h2 & mask) | g2;
    h3 = (h3 & mask) | g3;
    h4 = (h4 & mask) | g4;

    h0 = (h0      ) | (h1 << 26);
    h1 = (h1 >>  6) | (h2 << 20);
    h2 = (h2 >> 12) | (h3 << 14);
    h3 = (h3 >> 18) | (h4 <<  8);

    uint64_t f;
    f = (uint64_t)h0 + ctx->pad[0];             h0 = (uint32_t)f;
    f = (uint64_t)h1 + ctx->pad[1] + (f >> 32); h1 = (uint32_t)f;
    f = (uint64_t)h2 + ctx->pad[2] + (f >> 32); h2 = (uint32_t)f;
    f = (uint64_t)h3 + ctx->pad[3] + (f >> 32); h3 = (uint32_t)f;

    tag[0]  = (uint8_t)(h0      ); tag[1]  = (uint8_t)(h0 >>  8);
    tag[2]  = (uint8_t)(h0 >> 16); tag[3]  = (uint8_t)(h0 >> 24);
    tag[4]  = (uint8_t)(h1      ); tag[5]  = (uint8_t)(h1 >>  8);
    tag[6]  = (uint8_t)(h1 >> 16); tag[7]  = (uint8_t)(h1 >> 24);
    tag[8]  = (uint8_t)(h2      ); tag[9]  = (uint8_t)(h2 >>  8);
    tag[10] = (uint8_t)(h2 >> 16); tag[11] = (uint8_t)(h2 >> 24);
    tag[12] = (uint8_t)(h3      ); tag[13] = (uint8_t)(h3 >>  8);
    tag[14] = (uint8_t)(h3 >> 16); tag[15] = (uint8_t)(h3 >> 24);
}

void poly1305_mac(uint8_t tag[16], const uint8_t *msg, size_t len,
                  const uint8_t key[32])
{
    struct poly1305_ctx ctx;
    poly1305_init(&ctx, key);

    while (len >= 16) {
        poly1305_block(&ctx, msg, 1u << 24);
        msg += 16;
        len -= 16;
    }

    if (len > 0) {
        uint8_t last[16];
        memset(last, 0, 16);
        memcpy(last, msg, len);
        last[len] = 1;
        poly1305_block(&ctx, last, 0);
    }

    poly1305_finish(&ctx, tag);
}



static void aead_mac(uint8_t tag[16],
                     const uint8_t *aad, size_t aad_len,
                     const uint8_t *ct, size_t ct_len,
                     const uint8_t poly_key[32])
{
    struct poly1305_ctx ctx;
    poly1305_init(&ctx, poly_key);


    size_t aad_done = 0;
    while (aad_done + 16 <= aad_len) {
        poly1305_block(&ctx, aad + aad_done, 1u << 24);
        aad_done += 16;
    }
    if (aad_done < aad_len) {
        uint8_t last[16];
        memset(last, 0, 16);
        memcpy(last, aad + aad_done, aad_len - aad_done);
        poly1305_block(&ctx, last, 1u << 24);
    }


    size_t ct_done = 0;
    while (ct_done + 16 <= ct_len) {
        poly1305_block(&ctx, ct + ct_done, 1u << 24);
        ct_done += 16;
    }
    if (ct_done < ct_len) {
        uint8_t last[16];
        memset(last, 0, 16);
        memcpy(last, ct + ct_done, ct_len - ct_done);
        poly1305_block(&ctx, last, 1u << 24);
    }


    uint8_t lengths[16];
    uint64_t al = aad_len;
    uint64_t cl = ct_len;
    for (int i = 0; i < 8; i++) {
        lengths[i]     = (uint8_t)(al >> (i * 8));
        lengths[i + 8] = (uint8_t)(cl >> (i * 8));
    }
    poly1305_block(&ctx, lengths, 1u << 24);

    poly1305_finish(&ctx, tag);
}

int chacha20poly1305_encrypt(uint8_t *ct, uint8_t tag[16],
                             const uint8_t *pt, size_t pt_len,
                             const uint8_t *aad, size_t aad_len,
                             const uint8_t nonce[12],
                             const uint8_t key[32])
{
    uint8_t poly_key[32];

    if (!ct || !tag || !nonce || !key) return -1;
    if (pt_len > 0 && !pt) return -1;

    chacha20poly1305_derive_poly_key(poly_key, nonce, key);

    if (pt_len > 0) {
        chacha20_xor(ct, pt, pt_len, key, nonce, 1);
    }

    aead_mac(tag, aad, aad_len, ct, pt_len, poly_key);

    volatile uint8_t *p = poly_key;
    for (size_t i = 0; i < sizeof(poly_key); i++) p[i] = 0;

    return 0;
}

int chacha20poly1305_decrypt(uint8_t *pt,
                             const uint8_t *ct, size_t ct_len,
                             const uint8_t *aad, size_t aad_len,
                             const uint8_t tag[16],
                             const uint8_t nonce[12],
                             const uint8_t key[32])
{
    uint8_t poly_key[32];
    uint8_t computed_tag[16];
    int mismatch = 0;

    if (!pt || !ct || !tag || !nonce || !key) return -1;

    chacha20poly1305_derive_poly_key(poly_key, nonce, key);
    aead_mac(computed_tag, aad, aad_len, ct, ct_len, poly_key);

    for (int i = 0; i < 16; i++) {
        mismatch |= computed_tag[i] ^ tag[i];
    }

    volatile uint8_t *p = poly_key;
    for (size_t i = 0; i < sizeof(poly_key); i++) p[i] = 0;

    if (mismatch) {
        if (pt && ct_len > 0) {
            volatile uint8_t *q = pt;
            for (size_t i = 0; i < ct_len; i++) q[i] = 0;
        }
        return -1;
    }

    if (ct_len > 0) {
        chacha20_xor(pt, ct, ct_len, key, nonce, 1);
    }

    return 0;
}