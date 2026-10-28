/*
 * Lethal Kernel - EXT2 Filesystem
 *
 * v2.1: Full EXT2 implementation with indirect blocks, permissions,
 *       symlinks, buffer cache, and I/O scheduler.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_EXT2_H
#define LETHAL_EXT2_H

#include <stdint.h>
#include <stddef.h>
#include "../../core/mutex.h"

/* ============================================================================
 * EXT2 Constants
 * ============================================================================ */

#define EXT2_SUPER_MAGIC     0xEF53
#define EXT2_BLOCK_SIZE      1024
#define EXT2_MAX_BLOCKS      0xFFFFFFFF
#define EXT2_MAX_INODES      0xFFFFFFFF
#define EXT2_NAME_LEN        255

#define EXT2_DIRECT_BLOCKS   12
#define EXT2_INDIRECT_BLOCKS (EXT2_BLOCK_SIZE / 4)
#define EXT2_DINDIRECT_BLOCKS (EXT2_INDIRECT_BLOCKS * EXT2_INDIRECT_BLOCKS)
#define EXT2_TINDIRECT_BLOCKS (EXT2_DINDIRECT_BLOCKS * EXT2_INDIRECT_BLOCKS)

/* Inode types */
#define EXT2_FT_UNKNOWN      0
#define EXT2_FT_REG_FILE     1
#define EXT2_FT_DIR          2
#define EXT2_FT_CHRDEV       3
#define EXT2_FT_BLKDEV       4
#define EXT2_FT_FIFO         5
#define EXT2_FT_SOCK         6
#define EXT2_FT_SYMLINK      7

/* Inode flags */
#define EXT2_S_IFSOCK        0xC000
#define EXT2_S_IFLNK         0xA000
#define EXT2_S_IFREG         0x8000
#define EXT2_S_IFBLK         0x6000
#define EXT2_S_IFDIR         0x4000
#define EXT2_S_IFCHR         0x2000
#define EXT2_S_IFIFO         0x1000

#define EXT2_S_ISUID         0x0800
#define EXT2_S_ISGID         0x0400
#define EXT2_S_ISVTX         0x0200

#define EXT2_S_IRUSR         0x0100
#define EXT2_S_IWUSR         0x0080
#define EXT2_S_IXUSR         0x0040
#define EXT2_S_IRGRP         0x0020
#define EXT2_S_IWGRP         0x0010
#define EXT2_S_IXGRP         0x0008
#define EXT2_S_IROTH         0x0004
#define EXT2_S_IWOTH         0x0002
#define EXT2_S_IXOTH         0x0001

#define EXT2_S_IRWXU         0x01C0
#define EXT2_S_IRWXG         0x0038
#define EXT2_S_IRWXO         0x0007

/* Directory entry flags */
#define EXT2_DIR_REC_LEN     0xFF
#define EXT2_DIR_NAME_LEN    0xFF

/* ============================================================================
 * EXT2 Superblock (on-disk)
 * ============================================================================ */

struct ext2_superblock {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint32_t s_mnt_count;
    uint32_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algo_bitmap;
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_reserved_gdt_blocks;
    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_jnl_backup_type;
    uint16_t s_desc_size;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
    uint32_t s_mkfs_time;
    uint32_t s_jnl_blocks[17];
} __attribute__((packed));

/* ============================================================================
 * EXT2 Block Group Descriptor (on-disk)
 * ============================================================================ */

struct ext2_group_desc {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
} __attribute__((packed));

/* ============================================================================
 * EXT2 Inode (on-disk)
 * ============================================================================ */

struct ext2_inode {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15];
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint32_t i_osd2[3];
} __attribute__((packed));

/* ============================================================================
 * EXT2 Directory Entry (on-disk)
 * ============================================================================ */

struct ext2_dir_entry {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[0];
} __attribute__((packed));

/* ============================================================================
 * EXT2 In-core structures
 * ============================================================================ */

struct ext2_filesystem {
    uint32_t dev_id;
    struct ext2_superblock sb;
    struct ext2_group_desc *gdt;
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    uint32_t block_size;
    uint32_t inode_size;
    uint32_t groups_count;
    uint32_t first_data_block;
    void *block_cache;
    mutex_t lock;
    int mounted;
};

/* In-core inode structure (cached) */
struct ext2_inode_core {
    uint32_t inode_num;
    struct ext2_inode inode;
    uint32_t refcount;
    uint32_t flags;
    uint32_t block_group;
    uint32_t block_offset;
    struct ext2_filesystem *fs;
};

/* ============================================================================
 * Function Prototypes
 * ============================================================================ */

/* Mount/Unmount */
int ext2_mount(uint32_t dev_id);
int ext2_unmount(void);

/* Superblock */
int ext2_read_superblock(struct ext2_filesystem *fs);
int ext2_verify_superblock(struct ext2_superblock *sb);

/* Block I/O - defined in ext2_super.c */
int ext2_read_block(struct ext2_filesystem *fs, uint32_t block, void *buffer);
int ext2_write_block(struct ext2_filesystem *fs, uint32_t block, void *buffer);

/* Block Group */
int ext2_read_group_desc(struct ext2_filesystem *fs);
uint32_t ext2_get_group_number(uint32_t block);
uint32_t ext2_get_group_offset(uint32_t block);

/* Bitmap Operations */
int ext2_alloc_block(struct ext2_filesystem *fs);
int ext2_alloc_inode(struct ext2_filesystem *fs, uint32_t *inode_num);
void ext2_free_block(struct ext2_filesystem *fs, uint32_t block);
void ext2_free_inode(struct ext2_filesystem *fs, uint32_t inode_num);

/* Inode Operations */
int ext2_read_inode(struct ext2_filesystem *fs, uint32_t inode_num,
                     struct ext2_inode *inode);
int ext2_write_inode(struct ext2_filesystem *fs, uint32_t inode_num,
                      struct ext2_inode *inode);
int ext2_alloc_inode_core(struct ext2_filesystem *fs, uint32_t *inode_num);

/* Directory Operations */
int ext2_lookup(struct ext2_filesystem *fs, uint32_t dir_inode,
                 const char *name, uint32_t *inode_num);
int ext2_readdir(struct ext2_filesystem *fs, uint32_t dir_inode,
                  void (*callback)(const char *name, uint32_t inode, uint8_t type));
int ext2_mkdir(struct ext2_filesystem *fs, uint32_t parent_inode,
                const char *name, uint32_t mode);
int ext2_dir_write_entry(struct ext2_filesystem *fs, uint32_t dir_inode,
                          const char *name, uint32_t inode_num, uint8_t type);

/* File Operations */
int ext2_create_file(struct ext2_filesystem *fs, uint32_t parent_inode,
                      const char *name, uint32_t mode);
int ext2_read_file(struct ext2_filesystem *fs, uint32_t inode_num,
                    void *buffer, size_t offset, size_t count);
int ext2_write_file(struct ext2_filesystem *fs, uint32_t inode_num,
                     const void *buffer, size_t offset, size_t count);
int ext2_delete_file(struct ext2_filesystem *fs, uint32_t inode_num);

/* VFS Bindings */
int ext2_vfs_open(const char *path, uint32_t flags, void **handle);
long ext2_vfs_read(void *handle, void *buffer, size_t count);
long ext2_vfs_write(void *handle, const void *buffer, size_t count);
int ext2_vfs_close(void *handle);
long ext2_vfs_size(void *handle);
int ext2_vfs_mkdir(const char *path);

/* --- Indirect Block Operations (ext2_indirect.c) --- */
int ext2_get_block(struct ext2_filesystem *fs, struct ext2_inode *inode,
                    uint32_t block_index, uint32_t *block_num);
int ext2_allocate_block(struct ext2_filesystem *fs, struct ext2_inode *inode,
                         uint32_t block_index, uint32_t *block_num);
void ext2_free_all_blocks(struct ext2_filesystem *fs, struct ext2_inode *inode);

/* --- Permissions (ext2_permissions.c) --- */
int ext2_check_permission(struct ext2_inode *inode, uint16_t uid, uint16_t gid, int mode);
int ext2_set_permission(struct ext2_filesystem *fs, uint32_t inode_num, uint16_t mode);
int ext2_set_owner(struct ext2_filesystem *fs, uint32_t inode_num, uint16_t uid, uint16_t gid);
void ext2_mode_to_string(uint16_t mode, char *buffer, size_t size);

/* --- Symbolic Links (ext2_symlink.c) --- */
int ext2_symlink(struct ext2_filesystem *fs, uint32_t parent_inode,
                  const char *name, const char *target, uint32_t mode);
int ext2_read_symlink(struct ext2_filesystem *fs, uint32_t inode_num,
                       char *buffer, size_t size);
int ext2_hardlink(struct ext2_filesystem *fs, uint32_t dir_inode,
                   const char *name, uint32_t target_inode);

/* --- Buffer Cache (ext2_cache.c) --- */
void ext2_cache_init(void);
int ext2_cache_read(struct ext2_filesystem *fs, uint32_t block, void *buffer);
int ext2_cache_write(struct ext2_filesystem *fs, uint32_t block, void *buffer);
void ext2_cache_flush(struct ext2_filesystem *fs);
void ext2_cache_invalidate(uint32_t dev_id);

/* --- I/O Scheduler (ext2_io.c) --- */
void ext2_io_init(void);
int ext2_io_submit(uint32_t dev_id, uint32_t lba, uint32_t count,
                    uint8_t *buffer, int write);
int ext2_io_process(void);
void ext2_io_sync(void);
int ext2_io_readv(uint32_t dev_id, uint32_t lba, uint32_t count,
                   uint8_t **buffers, uint32_t *lengths, uint32_t num_bufs);

#endif /* LETHAL_EXT2_H */