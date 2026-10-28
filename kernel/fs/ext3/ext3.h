/*
 * Lethal Kernel - EXT3 Journaling Filesystem
 *
 * v3.0: Transaction-based journaling for crash recovery.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_EXT3_H
#define LETHAL_EXT3_H

#include <stdint.h>
#include <stddef.h>
#include "../../core/mutex.h"
#include "../ext2/ext2.h"

/* Journal magic number */
#define EXT3_JOURNAL_MAGIC     0xC03B3998

/* Journal states */
#define EXT3_JOURNAL_CLEAN     1
#define EXT3_JOURNAL_DIRTY     2
#define EXT3_JOURNAL_RECOVER   3

/* Journal block types */
#define EXT3_JOURNAL_DESCRIPTOR 1
#define EXT3_JOURNAL_COMMIT     2
#define EXT3_JOURNAL_REVOKE     3

/* Transaction flags */
#define EXT3_TRANS_STARTED      0x01
#define EXT3_TRANS_COMMITTED    0x02
#define EXT3_TRANS_FROZEN       0x04

/* Journal superblock (on-disk) */
struct ext3_journal_superblock {
    uint32_t j_magic;            /* 0xC03B3998 */
    uint32_t j_blocks;           /* Total blocks in journal */
    uint32_t j_first;            /* First block of journal */
    uint32_t j_sequence;         /* Last transaction ID */
    uint32_t j_start;            /* Start of current transaction */
    uint32_t j_errno;            /* Error code */
    uint8_t  j_uuid[16];         /* Filesystem UUID */
    uint32_t j_flags;            /* Flags */
    uint32_t j_max_trans;        /* Max transactions */
    uint32_t j_max_batch;        /* Max blocks per transaction */
    uint32_t j_reserved[12];
} __attribute__((packed));

/* Journal block header (on-disk) */
struct ext3_journal_header {
    uint32_t jh_magic;           /* 0xC03B3998 */
    uint32_t jh_type;            /* DESCRIPTOR, COMMIT, REVOKE */
    uint32_t jh_sequence;        /* Transaction ID */
    uint32_t jh_blocks;          /* Number of blocks in this transaction */
} __attribute__((packed));

/* Journal descriptor block (on-disk) */
struct ext3_journal_descriptor {
    struct ext3_journal_header header;
    uint32_t block_refs[0];      /* Array of block numbers to modify */
    /* Followed by block data */
} __attribute__((packed));

/* Journal commit block (on-disk) */
struct ext3_journal_commit {
    struct ext3_journal_header header;
    uint32_t jc_checksum;        /* CRC32 of the transaction */
} __attribute__((packed));

/* In-core transaction */
struct ext3_transaction {
    uint32_t sequence;            /* Transaction ID */
    uint32_t flags;               /* State flags */
    uint32_t block_count;         /* Number of blocks in transaction */
    uint32_t max_blocks;          /* Maximum blocks allowed */
    struct ext3_filesystem *fs;   /* Filesystem */
    mutex_t lock;                 /* Transaction lock */
};

/* EXT3 filesystem structure */
struct ext3_filesystem {
    struct ext2_filesystem ext2;  /* Base EXT2 filesystem */
    uint32_t journal_start;       /* Start block of journal area */
    uint32_t journal_blocks;      /* Size of journal in blocks */
    uint32_t journal_sequence;    /* Current transaction ID */
    struct ext3_transaction *current_trans;  /* Current open transaction */
    uint8_t journal_state;        /* CLEAN, DIRTY, RECOVER */
    mutex_t journal_lock;         /* Journal lock */
};

/* Mount/Unmount */
int ext3_mount(uint32_t dev_id);
int ext3_unmount(void);

/* Journal operations */
int ext3_journal_init(struct ext3_filesystem *fs);
int ext3_journal_start(struct ext3_filesystem *fs, uint32_t max_blocks);
int ext3_journal_stop(struct ext3_filesystem *fs);
int ext3_journal_add_block(struct ext3_filesystem *fs, uint32_t block, void *data);
int ext3_journal_commit(struct ext3_filesystem *fs);
int ext3_journal_replay(struct ext3_filesystem *fs);

/* VFS operations */
int ext3_vfs_open(const char *path, uint32_t flags, void **handle);
long ext3_vfs_read(void *handle, void *buffer, size_t count);
long ext3_vfs_write(void *handle, const void *buffer, size_t count);
int ext3_vfs_close(void *handle);
long ext3_vfs_size(void *handle);
int ext3_vfs_mkdir(const char *path);

#endif /* LETHAL_EXT3_H */