/*
 * Lethal Kernel - CSPRNG implementation
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "random.h"
#include "../../../core/console.h"
#include <string.h>

---------------------------------------------------------------- */

struct random_state {
    uint32_t key[8];        /* 256-bit key   */
    uint32_t nonce[3];      /* 96-bit nonce  */
    uint32_t counter_lo;    /* block counter */
    uint32_t counter_hi;
    uint8_t  buffer[64];    /* keystream buffer */
    size_t   buffer_pos;   
    int      initialized;
    int      has_rdrand;
    int      has_rdseed;
};

static struct random_state rng;


static void cpuid(uint32_t leaf, uint32_t subleaf,
                  uint32_t *eax, uint32_t *ebx,
                  uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile (
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf)
    );
}

static void detect_rdrand_rdseed(void)
{
    uint32_t eax, ebx, ecx, edx;

    /* CPUID leaf 1: ECX bit 30 = RDRAND */
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    rng.has_rdrand = (ecx >> 30) & 1;

    /* CPUID leaf 7, subleaf 0: EBX bit 18 = RDSEED */
    rng.has_rdseed = 0;
    if (eax >= 7) {
        cpuid(7, 0, &eax, &ebx, &ecx, &edx);
        rng.has_rdseed = (ebx >> 18) & 1;
    }

    serial_print("[random] RDRAND=");
    serial_print(rng.has_rdrand ? "yes" : "no");
    serial_print(" RDSEED=");
    serial_print(rng.has_rdseed ? "yes" : "no");
    serial_print("\n");
}

/* --------------------------------------------------------------------------
 * RDRAND / RDSEED
 * -------------------------------------------------------------------------- */

static int rdrand64(uint64_t *out)
{
    unsigned char ok;
    uint64_t val;

    if (!rng.has_rdrand) return 0;


    for (int i = 0; i < 10; i++) {
        __asm__ volatile (
            "rdrand %0; setc %1"
            : "=r"(val), "=qm"(ok)
            :
            : "cc"
        );
        if (ok) {
            *out = val;
            return 1;
        }
    }
    return 0;
}

static int rdseed64(uint64_t *out)
{
    unsigned char ok;
    uint64_t val;

    if (!rng.has_rdseed) return 0;

    for (int i = 0; i < 10; i++) {
        __asm__ volatile (
            "rdseed %0; setc %1"
            : "=r"(val), "=qm"(ok)
            :
            : "cc"
        );
        if (ok) {
            *out = val;
            return 1;
        }
    }
    return 0;
}


static uint64_t read_tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}



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

static void chacha20_block(const uint32_t in[16], uint32_t out[16])
{
    uint32_t x[16];

    for (int i = 0; i < 16; i++) x[i] = in[i];

    for (int i = 0; i < 10; i++) {
        /* Column rounds */
        QR(x[0], x[4], x[8],  x[12]);
        QR(x[1], x[5], x[9],  x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);
        /* Diagonal rounds */
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[8],  x[13]);
        QR(x[3], x[4], x[9],  x[14]);
    }

    for (int i = 0; i < 16; i++) out[i] = x[i] + in[i];
}

static void rng_refill(void)
{
    uint32_t state[16];

    /* Costanti "expand 32-byte k" */
    state[0]  = 0x61707865;
    state[1]  = 0x3320646e;
    state[2]  = 0x79622d32;
    state[3]  = 0x6b206574;
    state[4]  = rng.key[0];
    state[5]  = rng.key[1];
    state[6]  = rng.key[2];
    state[7]  = rng.key[3];
    state[8]  = rng.key[4];
    state[9]  = rng.key[5];
    state[10] = rng.key[6];
    state[11] = rng.key[7];
    state[12] = rng.counter_lo;
    state[13] = rng.counter_hi;
    state[14] = rng.nonce[0];
    state[15] = rng.nonce[1];

    chacha20_block(state, (uint32_t *)rng.buffer);

    rng.counter_lo++;
    if (rng.counter_lo == 0) rng.counter_hi++;

    rng.buffer_pos = 0;
}



static void rng_rekey(void)
{
    /* Usa 32 byte di output corrente come nuova chiave */
    uint32_t new_key[8];
    uint8_t *p = (uint8_t *)new_key;

    for (size_t i = 0; i < 32; i++) {
        if (rng.buffer_pos >= 64) rng_refill();
        p[i] = rng.buffer[rng.buffer_pos++];
    }

    for (int i = 0; i < 8; i++) rng.key[i] = new_key[i];


    rng.nonce[0] ^= (uint32_t)read_tsc();
    rng.nonce[1] ^= (uint32_t)(read_tsc() >> 32);
    rng.nonce[2] ^= (uint32_t)rng.counter_lo;

    rng.counter_lo = 0;
    rng.counter_hi = 0;
    rng.buffer_pos = 64; /* forza refill */
}


int random_init(void)
{
    uint64_t seed[6]; /* 48 byte di seed */
    int got_hw = 0;

    if (rng.initialized) return rng.has_rdrand ? 0 : 1;

    memset(&rng, 0, sizeof(rng));
    detect_rdrand_rdseed();

    /* Prova a ottenere seed da RDSEED (migliore) o RDRAND */
    for (int i = 0; i < 6; i++) {
        if (rdseed64(&seed[i])) {
            got_hw = 1;
            continue;
        }
        if (rdrand64(&seed[i])) {
            got_hw = 1;
            continue;
        }

        uint64_t tsc = read_tsc();
        uint64_t stack_addr;
        __asm__ volatile ("mov %%rsp, %0" : "=r"(stack_addr));
        seed[i] = tsc ^ (stack_addr << 17) ^ (uint64_t)i;
    }


    for (int i = 0; i < 8; i++) {
        rng.key[i] = (uint32_t)(seed[i % 6] >> ((i % 2) * 32));
    }

    for (int round = 0; round < 4; round++) {
        for (int i = 0; i < 8; i++) {
            rng.key[i] ^= rng.key[(i + 3) % 8] + (uint32_t)seed[(i + round) % 6];
        }
    }

    rng.nonce[0] = (uint32_t)seed[0];
    rng.nonce[1] = (uint32_t)(seed[1] >> 32);
    rng.nonce[2] = (uint32_t)seed[2];
    rng.counter_lo = 0;
    rng.counter_hi = 0;
    rng.buffer_pos = 64;
    rng.initialized = 1;


    for (int i = 0; i < 4; i++) {
        rng_refill();
        rng_rekey();
    }

    serial_print("[random] CSPRNG initialized (");
    serial_print(got_hw ? "hardware seed" : "TSC fallback");
    serial_print(")\n");

    return got_hw ? 0 : 1;
}

void random_add_entropy(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    if (!rng.initialized) random_init();

    /* XOR dell'entropia nella chiave, in modo semplice ma efficace:
     * ogni byte va in una posizione diversa della chiave in base al
     * contatore corrente. Non è HKDF ma è meglio che ignorarla. */
    for (size_t i = 0; i < len; i++) {
        size_t idx = (i + rng.buffer_pos) % 32;
        ((uint8_t *)rng.key)[idx] ^= p[i];
    }
    rng.buffer_pos = 64; /* forza refill */
}

void random_bytes(void *buf, size_t len)
{
    uint8_t *p = (uint8_t *)buf;

    if (!rng.initialized) random_init();

    while (len > 0) {
        if (rng.buffer_pos >= 64) {
            rng_refill();
            /* Rekey ogni 16 blocchi (1 KiB di output) per forward secrecy */
            if (rng.counter_lo % 16 == 0) rng_rekey();
        }
        size_t avail = 64 - rng.buffer_pos;
        size_t n = (len < avail) ? len : avail;
        memcpy(p, rng.buffer + rng.buffer_pos, n);
        p += n;
        len -= n;
        rng.buffer_pos += n;
    }
}

uint32_t random_u32(void)
{
    uint32_t v;
    random_bytes(&v, sizeof(v));
    return v;
}

uint64_t random_u64(void)
{
    uint64_t v;
    random_bytes(&v, sizeof(v));
    return v;
}

int random_has_rdrand(void) { return rng.has_rdrand; }
int random_has_rdseed(void) { return rng.has_rdseed; }