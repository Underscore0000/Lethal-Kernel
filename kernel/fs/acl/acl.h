/*
 * Lethal Kernel - Access Control Lists
 *
 * v4.0: POSIX ACLs for fine-grained permissions.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_ACL_H
#define LETHAL_ACL_H

#include <stdint.h>
#include <stddef.h>
#include "../../core/mutex.h"
#include "../ext2/ext2.h"

/* ============================================================================
 * ACL Constants
 * ============================================================================ */

#define ACL_ENTRY_MAX         64
#define ACL_EXTENDED_PERM     0x1000
#define ACL_MASK_READ         0x04
#define ACL_MASK_WRITE        0x02
#define ACL_MASK_EXECUTE      0x01

/* ACL Tag types */
#define ACL_TAG_USER          0x01
#define ACL_TAG_GROUP         0x02
#define ACL_TAG_MASK          0x04
#define ACL_TAG_OTHER         0x08

/* ACL Entry types */
#define ACL_ENTRY_ACCESS      0x01
#define ACL_ENTRY_DEFAULT     0x02

/* ============================================================================
 * ACL Structures
 * ============================================================================ */

/* ACL entry */
struct acl_entry {
    uint16_t tag_type;          /* USER, GROUP, MASK, OTHER */
    uint16_t permissions;       /* READ, WRITE, EXECUTE */
    uint32_t id;                /* UID or GID (0 for mask/other) */
} __attribute__((packed));

/* ACL header (on-disk) */
struct acl_header {
    uint32_t version;           /* ACL version */
    uint32_t entry_count;       /* Number of entries */
    uint32_t flags;             /* Access or Default */
    uint32_t checksum;          /* Integrity check */
} __attribute__((packed));

/* In-core ACL */
struct acl {
    uint32_t version;
    uint32_t entry_count;
    uint32_t flags;
    struct acl_entry entries[ACL_ENTRY_MAX];
    mutex_t lock;
};

/* ACL filesystem binding */
struct acl_fs {
    struct ext2_filesystem *fs;
    struct acl *cache;
    mutex_t cache_lock;
};

/* ============================================================================
 * Function Prototypes
 * ============================================================================ */

/* ACL Operations */
int acl_init(void);
struct acl *acl_alloc(void);
void acl_free(struct acl *acl);
int acl_add_entry(struct acl *acl, uint16_t tag, uint16_t perm, uint32_t id);
int acl_remove_entry(struct acl *acl, uint16_t tag, uint32_t id);
int acl_check_permission(struct acl *acl, uint16_t tag, uint32_t id, uint16_t perm);
int acl_validate(struct acl *acl);

/* ACL Storage (on-disk) */
int acl_read(struct ext2_filesystem *fs, uint32_t inode_num, struct acl **out);
int acl_write(struct ext2_filesystem *fs, uint32_t inode_num, struct acl *acl);
int acl_remove(struct ext2_filesystem *fs, uint32_t inode_num);

/* ACL VFS Integration */
int acl_vfs_check(struct ext2_inode *inode, uint16_t uid, uint16_t gid, int mode);
int acl_vfs_set(struct ext2_filesystem *fs, uint32_t inode_num, struct acl *acl);
int acl_vfs_get(struct ext2_filesystem *fs, uint32_t inode_num, struct acl **out);

/* Default ACLs */
int acl_set_default(struct ext2_filesystem *fs, uint32_t dir_inode, struct acl *acl);
int acl_get_default(struct ext2_filesystem *fs, uint32_t dir_inode, struct acl **out);

/* Utilities */
void acl_print(struct acl *acl);
int acl_to_string(struct acl *acl, char *buffer, size_t size);

#endif /* LETHAL_ACL_H */