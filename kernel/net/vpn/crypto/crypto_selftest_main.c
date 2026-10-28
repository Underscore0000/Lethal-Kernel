/*
 * Lethal Kernel - Crypto self-test host entry point
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "crypto_selftest.h"
#include "random.h"
#include <stdio.h>
#include <stdint.h>

int main(void)
{
    printf("Lethal Kernel - Crypto self-tests (host mode)\n");
    printf("=============================================\n\n");

    
    random_init();


    uint8_t rnd[16];
    random_bytes(rnd, sizeof(rnd));
    printf("[random] sample: ");
    for (int i = 0; i < 16; i++) printf("%02x", rnd[i]);
    printf("\n\n");

    int r = crypto_selftest_run();
    return (r == 0) ? 0 : 1;
}