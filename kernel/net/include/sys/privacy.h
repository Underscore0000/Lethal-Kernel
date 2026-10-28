/*
 * Lethal Kernel - Privacy System Calls (Userspace)
 *
 * v5.0: System call interface for privacy operations.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_SYS_PRIVACY_H
#define LETHAL_SYS_PRIVACY_H

#include <stdint.h>
#include "../linux/privacy.h"

/* ============================================================================
 * Privacy Syscall Numbers
 * ============================================================================ */

#define SYS_PRIVACY_GET   100
#define SYS_PRIVACY_SET   101
#define SYS_PRIVACY_LIST  102
#define SYS_PRIVACY_RESET 103

/* ============================================================================
 * Privacy Syscall Wrappers (for libc implementation)
 * ============================================================================ */

/* Get current privacy settings */
int sys_privacy_get(struct privacy_settings *settings);

/* Set privacy settings */
int sys_privacy_set(struct privacy_settings *settings);

/* List all privacy settings to console */
int sys_privacy_list(void);

/* Reset privacy settings to defaults */
int sys_privacy_reset(void);

/* ============================================================================
 * Userspace Privacy Functions (wrappers that call syscalls)
 * ============================================================================ */

static inline int privacy_get_settings(struct privacy_settings *settings)
{
    return sys_privacy_get(settings);
}

static inline int privacy_set_settings(struct privacy_settings *settings)
{
    return sys_privacy_set(settings);
}

static inline int privacy_list_settings(void)
{
    return sys_privacy_list();
}

static inline int privacy_reset_defaults(void)
{
    return sys_privacy_reset();
}

/* ============================================================================
 * Protection Level Helpers
 * ============================================================================ */

static inline const char *privacy_level_to_string(uint8_t level)
{
    switch(level) {
        case PRIVACY_LEVEL_BASIC:    return "basic";
        case PRIVACY_LEVEL_ADVANCED: return "advanced";
        case PRIVACY_LEVEL_PARANOID: return "paranoid";
        default:                     return "unknown";
    }
}

static inline int privacy_string_to_level(const char *str)
{
    if (strcmp(str, "basic") == 0) return PRIVACY_LEVEL_BASIC;
    if (strcmp(str, "advanced") == 0) return PRIVACY_LEVEL_ADVANCED;
    if (strcmp(str, "paranoid") == 0) return PRIVACY_LEVEL_PARANOID;
    return -1;
}

#endif /* LETHAL_SYS_PRIVACY_H */