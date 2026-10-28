/*
 * Lethal Kernel - Checksum Verification
 *
 * v4.0: Verify data integrity using checksums.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "checksum.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>

/* Initialize checksum context */
void checksum_init(struct checksum_context *ctx, uint32_t type)
{
    if (!ctx) return;
    
    memset(ctx, 0, sizeof(struct checksum_context));
    ctx->type = type;
    ctx->crc = 0xFFFFFFFF;
}

/* Update checksum with data */
void checksum_update(struct checksum_context *ctx, const uint8_t *data, size_t len)
{
    if (!ctx || !data || len == 0) return;
    
    switch (ctx->type) {
        case CHECKSUM_TYPE_CRC32:
            ctx->crc = checksum_crc32(ctx->crc, data, len);
            break;
        case CHECKSUM_TYPE_CRC32C:
            ctx->crc = checksum_crc32c(ctx->crc, data, len);
            break;
        default:
            break;
    }
    
    ctx->size += len;
    ctx->block_count = (ctx->size + CHECKSUM_BLOCK_SIZE - 1) / CHECKSUM_BLOCK_SIZE;
}

/* Finalize checksum */
uint32_t checksum_finalize(struct checksum_context *ctx)
{
    if (!ctx) return 0;
    return ~ctx->crc;
}

/* Compute checksum for a single block */
uint32_t checksum_block(uint32_t type, const uint8_t *data, size_t len)
{
    struct checksum_context ctx;
    checksum_init(&ctx, type);
    checksum_update(&ctx, data, len);
    return checksum_finalize(&ctx);
}

/* Verify a block against expected checksum */
int checksum_verify_block(uint32_t block_num, const uint8_t *data, size_t len, uint32_t expected)
{
    (void)expected; /* Silenzia il warning - parametro non usato per ora */
    
    /* Read checksum header from the block */
    struct checksum_header *header = (struct checksum_header *)data;
    
    if (header->magic != 0x4C4B4353) { /* 'LKCS' - Lethal Kernel Checksum */
        serial_print("[checksum] No checksum header found for block ");
        serial_print_dec(block_num);
        serial_print("\n");
        return 0;
    }
    
    uint32_t actual = checksum_block(header->type, data + sizeof(struct checksum_header),
                                     len - sizeof(struct checksum_header));
    
    return (actual == header->checksum);
}

/* Check block and return result */
struct checksum_result *checksum_check_block(uint32_t block_num, const uint8_t *data,
                                              size_t len, uint32_t expected)
{
    (void)expected; /* Silenzia il warning - parametro non usato per ora */
    
    struct checksum_result *result = (struct checksum_result *)kmalloc(sizeof(struct checksum_result));
    if (!result) return NULL;
    
    memset(result, 0, sizeof(struct checksum_result));
    result->block_num = block_num;
    result->expected = 0;
    result->valid = checksum_verify_block(block_num, data, len, 0);
    result->actual = checksum_block(CHECKSUM_TYPE_CRC32, data, len);
    result->type = CHECKSUM_TYPE_CRC32;
    
    return result;
}

/* Write checksum header to a block */
int checksum_write_header(uint32_t block_num, uint8_t *data, size_t len, uint32_t type)
{
    (void)block_num;
    (void)type;
    
    struct checksum_header *header = (struct checksum_header *)data;
    
    header->magic = 0x4C4B4353; /* 'LKCS' */
    header->type = CHECKSUM_TYPE_CRC32;
    header->block_num = block_num;
    header->timestamp = 0; /* Time will be added later */
    header->flags = 1; /* Valid */
    
    /* Compute checksum of the data */
    header->checksum = checksum_block(CHECKSUM_TYPE_CRC32, data + sizeof(struct checksum_header),
                                      len - sizeof(struct checksum_header));
    
    return 0;
}

/* Read checksum header from data */
int checksum_read_header(uint8_t *data, struct checksum_header *header)
{
    if (!data || !header) return -1;
    
    struct checksum_header *h = (struct checksum_header *)data;
    
    if (h->magic != 0x4C4B4353) {
        return -1;
    }
    
    memcpy(header, h, sizeof(struct checksum_header));
    return 0;
}

/* Verify checksum header */
int checksum_verify_header(struct checksum_header *header, const uint8_t *data, size_t len)
{
    if (!header || !data) return 0;
    
    if (header->magic != 0x4C4B4353) {
        return 0;
    }
    
    if (!(header->flags & 1)) {
        return 0; /* Invalid flag */
    }
    
    return checksum_verify_block(header->block_num, data, len, 0);
}

/* Validate superblock */
int checksum_validate_superblock(uint8_t *data, size_t len)
{
    struct checksum_header header;
    if (checksum_read_header(data, &header) != 0) {
        /* No checksum - assume valid */
        serial_print("[checksum] Superblock has no checksum\n");
        return 1;
    }
    
    return checksum_verify_header(&header, data, len);
}

/* Validate inode */
int checksum_validate_inode(uint8_t *data, size_t len)
{
    struct checksum_header header;
    if (checksum_read_header(data, &header) != 0) {
        return 1;
    }
    
    return checksum_verify_header(&header, data, len);
}

/* Validate directory entry */
int checksum_validate_dir_entry(uint8_t *data, size_t len)
{
    struct checksum_header header;
    if (checksum_read_header(data, &header) != 0) {
        return 1;
    }
    
    return checksum_verify_header(&header, data, len);
}

/* Print checksum result */
void checksum_result_print(struct checksum_result *result)
{
    if (!result) {
        serial_print("[checksum] NULL result\n");
        return;
    }
    
    serial_print("[checksum] Block ");
    serial_print_dec(result->block_num);
    serial_print(": expected=0x");
    serial_print_hex(result->expected);
    serial_print(" actual=0x");
    serial_print_hex(result->actual);
    serial_print(" ");
    serial_print(result->valid ? "VALID" : "CORRUPTED");
    serial_print("\n");
}

/* Get checksum type name */
const char *checksum_type_name(uint32_t type)
{
    switch (type) {
        case CHECKSUM_TYPE_NONE:   return "none";
        case CHECKSUM_TYPE_CRC32:  return "crc32";
        case CHECKSUM_TYPE_CRC32C: return "crc32c";
        case CHECKSUM_TYPE_MD5:    return "md5";
        default:                   return "unknown";
    }
}