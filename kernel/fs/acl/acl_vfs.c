/*
 * Lethal Kernel - ACL VFS Integration
 *
 * v4.0: VFS bindings for ACL permission checking.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "acl.h"
#include "../vfs.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>

/* Check permissions using ACL */
int acl_vfs_check(struct ext2_inode *inode, uint16_t uid, uint16_t gid, int mode)
{
    if (!inode) return 0;
    
    /* Root has all permissions */
    if (uid == 0) return 1;
    
    /* In a real implementation, we would read the ACL from the inode */
    /* For now, use standard UNIX permissions */
    
    /* Check owner permissions */
    if (uid == inode->i_uid) {
        if (mode & 0x01) return (inode->i_mode & EXT2_S_IXUSR) != 0;
        if (mode & 0x02) return (inode->i_mode & EXT2_S_IWUSR) != 0;
        return (inode->i_mode & EXT2_S_IRUSR) != 0;
    }
    
    /* Check group permissions */
    if (gid == inode->i_gid) {
        if (mode & 0x01) return (inode->i_mode & EXT2_S_IXGRP) != 0;
        if (mode & 0x02) return (inode->i_mode & EXT2_S_IWGRP) != 0;
        return (inode->i_mode & EXT2_S_IRGRP) != 0;
    }
    
    /* Others permissions */
    if (mode & 0x01) return (inode->i_mode & EXT2_S_IXOTH) != 0;
    if (mode & 0x02) return (inode->i_mode & EXT2_S_IWOTH) != 0;
    return (inode->i_mode & EXT2_S_IROTH) != 0;
}

/* Set ACL for an inode */
int acl_vfs_set(struct ext2_filesystem *fs, uint32_t inode_num, struct acl *acl)
{
    if (!fs || !acl) return -1;
    
    if (acl_validate(acl) != 0) {
        serial_print("[acl_vfs] Invalid ACL\n");
        return -1;
    }
    
    return acl_write(fs, inode_num, acl);
}

/* Get ACL for an inode */
int acl_vfs_get(struct ext2_filesystem *fs, uint32_t inode_num, struct acl **out)
{
    if (!fs || !out) return -1;
    return acl_read(fs, inode_num, out);
}

/* Set default ACL for a directory */
int acl_set_default(struct ext2_filesystem *fs, uint32_t dir_inode, struct acl *acl)
{
    if (!fs || !acl) return -1;
    
    /* Default ACLs apply to new files created in the directory */
    /* Store as an extended attribute on the directory */
    serial_print("[acl_vfs] Setting default ACL for directory ");
    serial_print_dec(dir_inode);
    serial_print("\n");
    
    return acl_write(fs, dir_inode, acl);
}

/* Get default ACL for a directory */
int acl_get_default(struct ext2_filesystem *fs, uint32_t dir_inode, struct acl **out)
{
    if (!fs || !out) return -1;
    return acl_read(fs, dir_inode, out);
}

/* VFS ACL operations structure */
struct vfs_acl_ops {
    int (*check)(struct ext2_inode *inode, uint16_t uid, uint16_t gid, int mode);
    int (*set)(struct ext2_filesystem *fs, uint32_t inode_num, struct acl *acl);
    int (*get)(struct ext2_filesystem *fs, uint32_t inode_num, struct acl **out);
    int (*set_default)(struct ext2_filesystem *fs, uint32_t dir_inode, struct acl *acl);
    int (*get_default)(struct ext2_filesystem *fs, uint32_t dir_inode, struct acl **out);
};

struct vfs_acl_ops acl_ops = {
    .check = acl_vfs_check,
    .set = acl_vfs_set,
    .get = acl_vfs_get,
    .set_default = acl_set_default,
    .get_default = acl_get_default
};

struct vfs_acl_ops *acl_get_ops(void)
{
    return &acl_ops;
}