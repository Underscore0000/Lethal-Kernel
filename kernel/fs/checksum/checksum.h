/*
 * Lethal Kernel - Checksum for Data Integrity
 *
 * v4.0: CRC32 checksums for metadata and data blocks.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_CHECKSUM_H
#define LETHAL_CHECKSUM_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * Checksum Constants
 * ============================================================================ */

#define CHECKSUM_TYPE_NONE      0
#define CHECKSUM_TYPE_CRC32     1
#define CHECKSUM_TYPE_CRC32C    2
#define CHECKSUM_TYPE_MD5       3

#define CHECKSUM_BLOCK_SIZE     4096
#define CHECKSUM_HASH_SIZE      4      /* CRC32 is 4 bytes */

/* ============================================================================
 * Checksum Structures
 * ============================================================================ */

/* Checksum header (stored with each block) */
struct checksum_header {
    uint32_t magic;             /* Magic number for verification */
    uint32_t type;              /* Checksum type (CRC32, etc.) */
    uint32_t block_num;         /* Block number this checksum covers */
    uint32_t checksum;          /* Actual checksum value */
    uint64_t timestamp;         /* When checksum was computed */
    uint32_t flags;             /* Flags (valid, corrupted, etc.) */
} __attribute__((packed));

/* Checksum context for computation */
struct checksum_context {
    uint32_t type;              /* Checksum type */
    uint32_t crc;               /* Current CRC value */
    uint64_t size;              /* Total size processed */
    uint32_t block_count;       /* Number of blocks processed */
};

/* Checksum verification result */
struct checksum_result {
    uint32_t expected;          /* Expected checksum */
    uint32_t actual;            /* Actual checksum */
    uint32_t block_num;         /* Block number checked */
    uint8_t  valid;             /* 1 if valid, 0 if corrupted */
    uint8_t  type;              /* Checksum type used */
};

/* ============================================================================
 * Function Prototypes
 * ============================================================================ */

/* CRC32 Core */
uint32_t checksum_crc32(uint32_t crc, const uint8_t *data, size_t len);
uint32_t checksum_crc32c(uint32_t crc, const uint8_t *data, size_t len);

/* Checksum Context */
void checksum_init(struct checksum_context *ctx, uint32_t type);
void checksum_update(struct checksum_context *ctx, const uint8_t *data, size_t len);
uint32_t checksum_finalize(struct checksum_context *ctx);

/* Block Operations */
uint32_t checksum_block(uint32_t type, const uint8_t *data, size_t len);
int checksum_verify_block(uint32_t block_num, const uint8_t *data, size_t len, uint32_t expected);
struct checksum_result *checksum_check_block(uint32_t block_num, const uint8_t *data, size_t len, uint32_t expected);

/* Header Operations */
int checksum_write_header(uint32_t block_num, uint8_t *data, size_t len, uint32_t type);
int checksum_read_header(uint8_t *data, struct checksum_header *header);
int checksum_verify_header(struct checksum_header *header, const uint8_t *data, size_t len);

/* Filesystem Integration */
int checksum_validate_superblock(uint8_t *data, size_t len);
int checksum_validate_inode(uint8_t *data, size_t len);
int checksum_validate_dir_entry(uint8_t *data, size_t len);

/* Utilities */
void checksum_result_print(struct checksum_result *result);
const char *checksum_type_name(uint32_t type);

#endif /* LETHAL_CHECKSUM_H */