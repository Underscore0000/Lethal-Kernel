/*
 * Lethal Kernel - MP Task Bootstrap
 *
 * v1.0: Bootstrap function for new multi-CPU tasks.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "scheduler_mp.h"

void mp_task_bootstrap_c(void (*entry)(void))
{
    entry();
    scheduler_mp_exit();
}