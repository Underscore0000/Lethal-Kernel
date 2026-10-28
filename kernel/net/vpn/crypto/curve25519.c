/*
 * Lethal Kernel - Curve25519 / X25519 implementation
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */

#include "curve25519.h"
#include <string.h>

typedef int32_t fe[10];

/* ------------------------------------------------------------------ */
/* Load helpers (little-endian)                                        */
/* ------------------------------------------------------------------ */

static int64_t load_3(const uint8_t *in)
{
    return (int64_t)in[0]
         | ((int64_t)in[1] << 8)
         | ((int64_t)in[2] << 16);
}

static int64_t load_4(const uint8_t *in)
{
    return (int64_t)in[0]
         | ((int64_t)in[1] << 8)
         | ((int64_t)in[2] << 16)
         | ((int64_t)in[3] << 24);
}

/* ------------------------------------------------------------------ */
/* fe <-> bytes (ref10 fedele)                                         */
/* ------------------------------------------------------------------ */

static void fe_frombytes(fe h, const uint8_t *s)
{
    int64_t h0 = load_4(s);
    int64_t h1 = load_3(s + 4) << 6;
    int64_t h2 = load_3(s + 7) << 5;
    int64_t h3 = load_3(s + 10) << 3;
    int64_t h4 = load_3(s + 13) << 2;
    int64_t h5 = load_4(s + 16);
    int64_t h6 = load_3(s + 20) << 7;
    int64_t h7 = load_3(s + 23) << 5;
    int64_t h8 = load_3(s + 26) << 4;
    int64_t h9 = (load_3(s + 29) & 8388607) << 2;

    int64_t carry9 = (h9 + ((int64_t)1 << 24)) >> 25;
    h0 += carry9 * 19;
    h9 -= carry9 << 25;

    int64_t carry1 = (h1 + ((int64_t)1 << 24)) >> 25;
    h2 += carry1;
    h1 -= carry1 << 25;

    int64_t carry3 = (h3 + ((int64_t)1 << 24)) >> 25;
    h4 += carry3;
    h3 -= carry3 << 25;

    int64_t carry5 = (h5 + ((int64_t)1 << 24)) >> 25;
    h6 += carry5;
    h5 -= carry5 << 25;

    int64_t carry7 = (h7 + ((int64_t)1 << 24)) >> 25;
    h8 += carry7;
    h7 -= carry7 << 25;

    int64_t carry0 = (h0 + ((int64_t)1 << 25)) >> 26;
    h1 += carry0;
    h0 -= carry0 << 26;

    int64_t carry2 = (h2 + ((int64_t)1 << 25)) >> 26;
    h3 += carry2;
    h2 -= carry2 << 26;

    int64_t carry4 = (h4 + ((int64_t)1 << 25)) >> 26;
    h5 += carry4;
    h4 -= carry4 << 26;

    int64_t carry6 = (h6 + ((int64_t)1 << 25)) >> 26;
    h7 += carry6;
    h6 -= carry6 << 26;

    int64_t carry8 = (h8 + ((int64_t)1 << 25)) >> 26;
    h9 += carry8;
    h8 -= carry8 << 26;

    h[0] = (int32_t)h0;
    h[1] = (int32_t)h1;
    h[2] = (int32_t)h2;
    h[3] = (int32_t)h3;
    h[4] = (int32_t)h4;
    h[5] = (int32_t)h5;
    h[6] = (int32_t)h6;
    h[7] = (int32_t)h7;
    h[8] = (int32_t)h8;
    h[9] = (int32_t)h9;
}

static void fe_tobytes(uint8_t *s, const fe h_in)
{
    fe h;
    int32_t q;
    int32_t c;

    for (int i = 0; i < 10; i++)
        h[i] = h_in[i];


    c = (h[0] + (1 << 25)) >> 26; h[1] += c; h[0] -= c << 26;
    c = (h[4] + (1 << 25)) >> 26; h[5] += c; h[4] -= c << 26;
    c = (h[1] + (1 << 24)) >> 25; h[2] += c; h[1] -= c << 25;
    c = (h[5] + (1 << 24)) >> 25; h[6] += c; h[5] -= c << 25;
    c = (h[2] + (1 << 25)) >> 26; h[3] += c; h[2] -= c << 26;
    c = (h[6] + (1 << 25)) >> 26; h[7] += c; h[6] -= c << 26;
    c = (h[3] + (1 << 24)) >> 25; h[4] += c; h[3] -= c << 25;
    c = (h[7] + (1 << 24)) >> 25; h[8] += c; h[7] -= c << 25;
    c = (h[4] + (1 << 25)) >> 26; h[5] += c; h[4] -= c << 26;
    c = (h[8] + (1 << 25)) >> 26; h[9] += c; h[8] -= c << 26;
    c = (h[9] + (1 << 24)) >> 25; h[0] += c * 19; h[9] -= c << 25;
    c = (h[0] + (1 << 25)) >> 26; h[1] += c; h[0] -= c << 26;


    q = (19 * h[9] + (1 << 24)) >> 25;
    q = (h[0] + q) >> 26;
    q = (h[1] + q) >> 25;
    q = (h[2] + q) >> 26;
    q = (h[3] + q) >> 25;
    q = (h[4] + q) >> 26;
    q = (h[5] + q) >> 25;
    q = (h[6] + q) >> 26;
    q = (h[7] + q) >> 25;
    q = (h[8] + q) >> 26;
    q = (h[9] + q) >> 25;

    h[0] += 19 * q;

    c = h[0] >> 26; h[1] += c; h[0] -= c << 26;
    c = h[1] >> 25; h[2] += c; h[1] -= c << 25;
    c = h[2] >> 26; h[3] += c; h[2] -= c << 26;
    c = h[3] >> 25; h[4] += c; h[3] -= c << 25;
    c = h[4] >> 26; h[5] += c; h[4] -= c << 26;
    c = h[5] >> 25; h[6] += c; h[5] -= c << 25;
    c = h[6] >> 26; h[7] += c; h[6] -= c << 26;
    c = h[7] >> 25; h[8] += c; h[7] -= c << 25;
    c = h[8] >> 26; h[9] += c; h[8] -= c << 26;
    c = h[9] >> 25; h[0] += c * 19; h[9] -= c << 25;
    c = h[0] >> 26; h[1] += c; h[0] -= c << 26;

    s[0]  = (uint8_t)(h[0]);
    s[1]  = (uint8_t)(h[0] >> 8);
    s[2]  = (uint8_t)(h[0] >> 16);
    s[3]  = (uint8_t)((h[0] >> 24) | (h[1] << 2));
    s[4]  = (uint8_t)(h[1] >> 6);
    s[5]  = (uint8_t)(h[1] >> 14);
    s[6]  = (uint8_t)((h[1] >> 22) | (h[2] << 3));
    s[7]  = (uint8_t)(h[2] >> 5);
    s[8]  = (uint8_t)(h[2] >> 13);
    s[9]  = (uint8_t)((h[2] >> 21) | (h[3] << 5));
    s[10] = (uint8_t)(h[3] >> 3);
    s[11] = (uint8_t)(h[3] >> 11);
    s[12] = (uint8_t)((h[3] >> 19) | (h[4] << 6));
    s[13] = (uint8_t)(h[4] >> 2);
    s[14] = (uint8_t)(h[4] >> 10);
    s[15] = (uint8_t)(h[4] >> 18);
    s[16] = (uint8_t)(h[5]);
    s[17] = (uint8_t)(h[5] >> 8);
    s[18] = (uint8_t)(h[5] >> 16);
    s[19] = (uint8_t)((h[5] >> 24) | (h[6] << 1));
    s[20] = (uint8_t)(h[6] >> 7);
    s[21] = (uint8_t)(h[6] >> 15);
    s[22] = (uint8_t)((h[6] >> 23) | (h[7] << 3));
    s[23] = (uint8_t)(h[7] >> 5);
    s[24] = (uint8_t)(h[7] >> 13);
    s[25] = (uint8_t)((h[7] >> 21) | (h[8] << 4));
    s[26] = (uint8_t)(h[8] >> 4);
    s[27] = (uint8_t)(h[8] >> 12);
    s[28] = (uint8_t)((h[8] >> 20) | (h[9] << 6));
    s[29] = (uint8_t)(h[9] >> 2);
    s[30] = (uint8_t)(h[9] >> 10);
    s[31] = (uint8_t)(h[9] >> 18);
}



static void fe_0(fe h) { for (int i = 0; i < 10; i++) h[i] = 0; }
static void fe_1(fe h) { h[0] = 1; for (int i = 1; i < 10; i++) h[i] = 0; }
static void fe_copy(fe h, const fe f) { for (int i = 0; i < 10; i++) h[i] = f[i]; }

static void fe_add(fe h, const fe f, const fe g)
{
    for (int i = 0; i < 10; i++)
        h[i] = f[i] + g[i];
}

static void fe_sub(fe h, const fe f, const fe g)
{
    for (int i = 0; i < 10; i++)
        h[i] = f[i] - g[i];
}

static void fe_mul(fe h, const fe f, const fe g)
{
    int32_t f0 = f[0], f1 = f[1], f2 = f[2], f3 = f[3], f4 = f[4];
    int32_t f5 = f[5], f6 = f[6], f7 = f[7], f8 = f[8], f9 = f[9];
    int32_t g0 = g[0], g1 = g[1], g2 = g[2], g3 = g[3], g4 = g[4];
    int32_t g5 = g[5], g6 = g[6], g7 = g[7], g8 = g[8], g9 = g[9];

    int32_t g1_19 = 19 * g1, g2_19 = 19 * g2, g3_19 = 19 * g3;
    int32_t g4_19 = 19 * g4, g5_19 = 19 * g5, g6_19 = 19 * g6;
    int32_t g7_19 = 19 * g7, g8_19 = 19 * g8, g9_19 = 19 * g9;

    int32_t f1_2 = 2 * f1, f3_2 = 2 * f3, f5_2 = 2 * f5;
    int32_t f7_2 = 2 * f7, f9_2 = 2 * f9;

    int64_t h0 = (int64_t)f0*g0 + (int64_t)f1_2*g9_19
               + (int64_t)f2*g8_19 + (int64_t)f3_2*g7_19
               + (int64_t)f4*g6_19 + (int64_t)f5_2*g5_19
               + (int64_t)f6*g4_19 + (int64_t)f7_2*g3_19
               + (int64_t)f8*g2_19 + (int64_t)f9_2*g1_19;

    int64_t h1 = (int64_t)f0*g1 + (int64_t)f1*g0
               + (int64_t)f2*g9_19 + (int64_t)f3*g8_19
               + (int64_t)f4*g7_19 + (int64_t)f5*g6
               + (int64_t)f6*g5 + (int64_t)f7*g4
               + (int64_t)f8*g3 + (int64_t)f9*g2;

    int64_t h2 = (int64_t)f0*g2 + (int64_t)f1_2*g1
               + (int64_t)f2*g0 + (int64_t)f3_2*g9_19
               + (int64_t)f4*g8_19 + (int64_t)f5_2*g7_19
               + (int64_t)f6*g6 + (int64_t)f7_2*g5
               + (int64_t)f8*g4 + (int64_t)f9_2*g3;

    int64_t h3 = (int64_t)f0*g3 + (int64_t)f1*g2
               + (int64_t)f2*g1 + (int64_t)f3*g0
               + (int64_t)f4*g9_19 + (int64_t)f5*g8
               + (int64_t)f6*g7 + (int64_t)f7*g6
               + (int64_t)f8*g5 + (int64_t)f9*g4;

    int64_t h4 = (int64_t)f0*g4 + (int64_t)f1_2*g3
               + (int64_t)f2*g2 + (int64_t)f3_2*g1
               + (int64_t)f4*g0 + (int64_t)f5_2*g9_19
               + (int64_t)f6*g8_19 + (int64_t)f7_2*g7_19
               + (int64_t)f8*g6 + (int64_t)f9_2*g5;

    int64_t h5 = (int64_t)f0*g5 + (int64_t)f1*g4
               + (int64_t)f2*g3 + (int64_t)f3*g2
               + (int64_t)f4*g1 + (int64_t)f5*g0
               + (int64_t)f6*g9_19 + (int64_t)f7*g8_19
               + (int64_t)f8*g7_19 + (int64_t)f9*g6_19;

    int64_t h6 = (int64_t)f0*g6 + (int64_t)f1_2*g5
               + (int64_t)f2*g4 + (int64_t)f3_2*g3
               + (int64_t)f4*g2 + (int64_t)f5_2*g1
               + (int64_t)f6*g0 + (int64_t)f7_2*g9_19
               + (int64_t)f8*g8_19 + (int64_t)f9_2*g7_19;

    int64_t h7 = (int64_t)f0*g7 + (int64_t)f1*g6
               + (int64_t)f2*g5 + (int64_t)f3*g4
               + (int64_t)f4*g3 + (int64_t)f5*g2
               + (int64_t)f6*g1 + (int64_t)f7*g0
               + (int64_t)f8*g9_19 + (int64_t)f9*g8_19;

    int64_t h8 = (int64_t)f0*g8 + (int64_t)f1_2*g7
               + (int64_t)f2*g6 + (int64_t)f3_2*g5
               + (int64_t)f4*g4 + (int64_t)f5_2*g3
               + (int64_t)f6*g2 + (int64_t)f7_2*g1
               + (int64_t)f8*g0 + (int64_t)f9_2*g9_19;

    int64_t h9 = (int64_t)f0*g9 + (int64_t)f1*g8
               + (int64_t)f2*g7 + (int64_t)f3*g6
               + (int64_t)f4*g5 + (int64_t)f5*g4
               + (int64_t)f6*g3 + (int64_t)f7*g2
               + (int64_t)f8*g1 + (int64_t)f9*g0;

    int64_t c;

    c = (h0 + ((int64_t)1 << 25)) >> 26; h1 += c; h0 -= c << 26;
    c = (h4 + ((int64_t)1 << 25)) >> 26; h5 += c; h4 -= c << 26;
    c = (h1 + ((int64_t)1 << 24)) >> 25; h2 += c; h1 -= c << 25;
    c = (h5 + ((int64_t)1 << 24)) >> 25; h6 += c; h5 -= c << 25;
    c = (h2 + ((int64_t)1 << 25)) >> 26; h3 += c; h2 -= c << 26;
    c = (h6 + ((int64_t)1 << 25)) >> 26; h7 += c; h6 -= c << 26;
    c = (h3 + ((int64_t)1 << 24)) >> 25; h4 += c; h3 -= c << 25;
    c = (h7 + ((int64_t)1 << 24)) >> 25; h8 += c; h7 -= c << 25;
    c = (h4 + ((int64_t)1 << 25)) >> 26; h5 += c; h4 -= c << 26;
    c = (h8 + ((int64_t)1 << 25)) >> 26; h9 += c; h8 -= c << 26;
    c = (h9 + ((int64_t)1 << 24)) >> 25; h0 += c * 19; h9 -= c << 25;
    c = (h0 + ((int64_t)1 << 25)) >> 26; h1 += c; h0 -= c << 26;

    h[0] = (int32_t)h0;
    h[1] = (int32_t)h1;
    h[2] = (int32_t)h2;
    h[3] = (int32_t)h3;
    h[4] = (int32_t)h4;
    h[5] = (int32_t)h5;
    h[6] = (int32_t)h6;
    h[7] = (int32_t)h7;
    h[8] = (int32_t)h8;
    h[9] = (int32_t)h9;
}

static void fe_sq(fe h, const fe f) { fe_mul(h, f, f); }

static void fe_invert(fe out, const fe z)
{
    fe t0, t1, t2, t3;
    int i;

    fe_sq(t0, z);
    fe_sq(t1, t0);
    fe_sq(t1, t1);
    fe_mul(t1, z, t1);
    fe_mul(t0, t0, t1);
    fe_sq(t2, t0);
    fe_mul(t1, t1, t2);
    fe_sq(t2, t1);
    for (i = 1; i < 5; i++) fe_sq(t2, t2);
    fe_mul(t1, t2, t1);
    fe_sq(t2, t1);
    for (i = 1; i < 10; i++) fe_sq(t2, t2);
    fe_mul(t2, t2, t1);
    fe_sq(t3, t2);
    for (i = 1; i < 20; i++) fe_sq(t3, t3);
    fe_mul(t2, t3, t2);
    fe_sq(t2, t2);
    for (i = 1; i < 10; i++) fe_sq(t2, t2);
    fe_mul(t1, t2, t1);
    fe_sq(t2, t1);
    for (i = 1; i < 50; i++) fe_sq(t2, t2);
    fe_mul(t2, t2, t1);
    fe_sq(t3, t2);
    for (i = 1; i < 100; i++) fe_sq(t3, t3);
    fe_mul(t2, t3, t2);
    fe_sq(t2, t2);
    for (i = 1; i < 50; i++) fe_sq(t2, t2);
    fe_mul(t1, t2, t1);
    fe_sq(t1, t1);
    for (i = 1; i < 5; i++) fe_sq(t1, t1);
    fe_mul(out, t1, t0);
}

static void fe_cswap(fe f, fe g, uint32_t b)
{
    int32_t mask = -(int32_t)b;
    for (int i = 0; i < 10; i++) {
        int32_t x = f[i];
        int32_t y = g[i];
        int32_t t = mask & (x ^ y);
        f[i] = x ^ t;
        g[i] = y ^ t;
    }
}

/* ------------------------------------------------------------------ */
/* X25519                                                              */
/* ------------------------------------------------------------------ */

static void scalar_clamp(uint8_t scalar[32])
{
    scalar[0]  &= 248;
    scalar[31] &= 127;
    scalar[31] |= 64;
}

void curve25519_scalarmult(uint8_t out[32],
                           const uint8_t scalar[32],
                           const uint8_t u[32])
{
    uint8_t e[32];
    fe x1, x2, z2, x3, z3;
    fe A, AA, B, BB, E, C, D, DA, CB, t;
    fe a24;
    int pos;
    uint32_t swap = 0;

    memcpy(e, scalar, 32);
    scalar_clamp(e);

    fe_frombytes(x1, u);
    fe_1(x2);
    fe_0(z2);
    fe_copy(x3, x1);
    fe_1(z3);

    fe_0(a24);
    a24[0] = 121665;

    for (pos = 254; pos >= 0; --pos) {
        uint32_t b = (e[pos / 8] >> (pos & 7)) & 1;

        swap ^= b;
        fe_cswap(x2, x3, swap);
        fe_cswap(z2, z3, swap);
        swap = b;

        fe_add(A, x2, z2);
        fe_sub(B, x2, z2);
        fe_sq(AA, A);
        fe_sq(BB, B);
        fe_sub(E, AA, BB);
        fe_add(C, x3, z3);
        fe_sub(D, x3, z3);
        fe_mul(DA, D, A);
        fe_mul(CB, C, B);

        fe_add(x3, DA, CB);
        fe_sq(x3, x3);
        fe_sub(z3, DA, CB);
        fe_sq(z3, z3);
        fe_mul(z3, x1, z3);

        fe_mul(x2, AA, BB);
        fe_mul(t, E, a24);
        fe_add(t, AA, t);
        fe_mul(z2, E, t);
    }

    fe_cswap(x2, x3, swap);
    fe_cswap(z2, z3, swap);

    fe z2_inv, result;
    fe_invert(z2_inv, z2);
    fe_mul(result, x2, z2_inv);

    fe_tobytes(out, result);

    memset(e, 0, sizeof(e));
    memset(x2, 0, sizeof(x2));
    memset(z2, 0, sizeof(z2));
    memset(DA, 0, sizeof(DA));
    memset(CB, 0, sizeof(CB));
    memset(t,  0, sizeof(t));
    memset(result, 0, sizeof(result));
    memset(z2_inv, 0, sizeof(z2_inv));
}

const uint8_t curve25519_basepoint[32] = {
    9, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

void curve25519_public_key(uint8_t pk[32], const uint8_t sk[32])
{
    curve25519_scalarmult(pk, sk, curve25519_basepoint);
}

int curve25519_shared(uint8_t shared[32],
                      const uint8_t sk[32],
                      const uint8_t peer_pk[32])
{
    if (!shared || !sk || !peer_pk)
        return -1;

    curve25519_scalarmult(shared, sk, peer_pk);

    uint8_t acc = 0;
    for (int i = 0; i < 32; i++)
        acc |= shared[i];

    if (acc == 0)
        return -1;

    return 0;
}

int curve25519_is_valid_public_key(const uint8_t pk[32])
{
    static const uint8_t low_order[7][32] = {
        { 0 },
        { 1 },
        { 0xe0,0xeb,0x7a,0x7c,0x3b,0x41,0xb8,0xae,
          0x16,0x56,0xe3,0xfa,0xf1,0x9f,0xc4,0x6a,
          0xda,0x09,0x8d,0xeb,0x9c,0x32,0xb1,0xfd,
          0x86,0x62,0x05,0x16,0x5f,0x49,0xb8,0x00 },
        { 0x5f,0x9c,0x95,0xbc,0xa3,0x50,0x8c,0x24,
          0xb1,0xd0,0xb1,0x55,0x9c,0x83,0xef,0x5b,
          0x04,0x44,0x5c,0xc4,0x58,0x1c,0x8e,0x86,
          0xd8,0x22,0x4e,0xdd,0xd0,0x9f,0x11,0x57 },
        { 0xec,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
          0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
          0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
          0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f },
        { 0xed,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
          0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
          0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
          0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f },
        { 0xee,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
          0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
          0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
          0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f }
    };

    for (int i = 0; i < 7; i++) {
        uint8_t diff = 0;
        for (int j = 0; j < 32; j++)
            diff |= (uint8_t)(pk[j] ^ low_order[i][j]);
        if (diff == 0)
            return 0;
    }
    return 1;
}