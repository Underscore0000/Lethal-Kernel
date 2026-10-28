/*
 * Lethal Kernel - EXT3 Journal Manager
 *
 * v3.0: Transaction management, block logging, and commit.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ext3.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>

/* Forward declarations for EXT2 block I/O */
extern int ext2_read_block(struct ext2_filesystem *fs, uint32_t block, void *buffer);
extern int ext2_write_block(struct ext2_filesystem *fs, uint32_t block, void *buffer);
extern int ext2_alloc_block(struct ext2_filesystem *fs);

static struct ext3_filesystem *ext3_fs = NULL;

/* Read journal superblock */
static int ext3_read_journal_sb(struct ext3_filesystem *fs)
{
    uint8_t *buffer = (uint8_t *)kmalloc(fs->ext2.block_size);
    if (!buffer) return -1;
    
    /* Journal superblock is at first block of journal area */
    if (ext2_read_block(&fs->ext2, fs->journal_start, buffer) != 0) {
        kfree(buffer);
        return -1;
    }
    
    struct ext3_journal_superblock *jsb = (struct ext3_journal_superblock *)buffer;
    if (jsb->j_magic != EXT3_JOURNAL_MAGIC) {
        /* Initialize a new journal */
        memset(jsb, 0, sizeof(struct ext3_journal_superblock));
        jsb->j_magic = EXT3_JOURNAL_MAGIC;
        jsb->j_blocks = fs->journal_blocks;
        jsb->j_first = fs->journal_start + 1;
        jsb->j_sequence = 1;
        jsb->j_start = 1;
        jsb->j_flags = EXT3_JOURNAL_CLEAN;
        jsb->j_max_trans = 1024;
        jsb->j_max_batch = 256;
        ext2_write_block(&fs->ext2, fs->journal_start, buffer);
        serial_print("[ext3] New journal initialized\n");
    } else {
        fs->journal_sequence = jsb->j_sequence;
        fs->journal_state = jsb->j_flags;
        serial_print("[ext3] Journal found: sequence ");
        serial_print_dec(jsb->j_sequence);
        serial_print(" state=");
        serial_print_dec(jsb->j_flags);
        serial_print("\n");
    }
    
    kfree(buffer);
    return 0;
}

/* Write journal superblock */
int ext3_write_journal_sb(struct ext3_filesystem *fs)
{
    uint8_t *buffer = (uint8_t *)kmalloc(fs->ext2.block_size);
    if (!buffer) return -1;
    
    if (ext2_read_block(&fs->ext2, fs->journal_start, buffer) != 0) {
        kfree(buffer);
        return -1;
    }
    
    struct ext3_journal_superblock *jsb = (struct ext3_journal_superblock *)buffer;
    jsb->j_sequence = fs->journal_sequence;
    jsb->j_start = 1; /* Simplified: always start at block 1 */
    jsb->j_flags = fs->journal_state;
    
    ext2_write_block(&fs->ext2, fs->journal_start, buffer);
    kfree(buffer);
    return 0;
}

/* Initialize journal */
int ext3_journal_init(struct ext3_filesystem *fs)
{
    serial_print("[ext3_journal] Initializing...\n");
    
    /* Allocate journal area if not already allocated */
    if (fs->journal_blocks == 0) {
        /* Reserve 4MB for journal (4096 blocks) */
        fs->journal_blocks = 4096;
        fs->journal_start = fs->ext2.sb.s_blocks_count - fs->journal_blocks;
        serial_print("[ext3_journal] Reserved ");
        serial_print_dec(fs->journal_blocks);
        serial_print(" blocks at ");
        serial_print_dec(fs->journal_start);
        serial_print("\n");
    }
    
    mutex_init(&fs->journal_lock);
    fs->journal_state = EXT3_JOURNAL_DIRTY;
    fs->current_trans = NULL;
    
    /* Read or initialize journal superblock */
    if (ext3_read_journal_sb(fs) != 0) {
        serial_print("[ext3_journal] Failed to read journal\n");
        return -1;
    }
    
    /* Check if we need to replay */
    if (fs->journal_state == EXT3_JOURNAL_DIRTY) {
        serial_print("[ext3_journal] Journal is dirty, replaying...\n");
        ext3_journal_replay(fs);
    }
    
    serial_print("[ext3_journal] Initialized\n");
    return 0;
}

/* Start a transaction */
int ext3_journal_start(struct ext3_filesystem *fs, uint32_t max_blocks)
{
    if (!fs) return -1;
    
    mutex_lock(&fs->journal_lock);
    
    /* Check if there's already a transaction */
    if (fs->current_trans) {
        mutex_unlock(&fs->journal_lock);
        return -1; /* Transaction already in progress */
    }
    
    /* Allocate transaction */
    struct ext3_transaction *trans = (struct ext3_transaction *)kmalloc(sizeof(struct ext3_transaction));
    if (!trans) {
        mutex_unlock(&fs->journal_lock);
        return -1;
    }
    
    memset(trans, 0, sizeof(struct ext3_transaction));
    trans->sequence = fs->journal_sequence++;
    trans->flags = EXT3_TRANS_STARTED;
    trans->max_blocks = max_blocks;
    trans->block_count = 0;
    trans->fs = fs;
    mutex_init(&trans->lock);
    
    fs->current_trans = trans;
    fs->journal_state = EXT3_JOURNAL_DIRTY;
    
    /* Mark journal as dirty in superblock */
    ext3_write_journal_sb(fs);
    
    serial_print("[ext3_journal] Started transaction ");
    serial_print_dec(trans->sequence);
    serial_print("\n");
    
    mutex_unlock(&fs->journal_lock);
    return 0;
}

/* Add a block to the current transaction */
int ext3_journal_add_block(struct ext3_filesystem *fs, uint32_t block, void *data)
{
    (void)block; /* Silenzia il warning - parametro non usato per ora */
    (void)data;  /* Silenzia il warning - parametro non usato per ora */
    
    if (!fs || !fs->current_trans) return -1;
    
    struct ext3_transaction *trans = fs->current_trans;
    
    mutex_lock(&trans->lock);
    
    if (trans->block_count >= trans->max_blocks) {
        mutex_unlock(&trans->lock);
        return -1; /* Transaction full */
    }
    
    /* For now, we just store a reference. In a real implementation,
     * we would copy the data to a journal buffer. */
    trans->block_count++;
    
    mutex_unlock(&trans->lock);
    return 0;
}

/* Commit the current transaction */
int ext3_journal_commit(struct ext3_filesystem *fs)
{
    if (!fs || !fs->current_trans) return -1;
    
    struct ext3_transaction *trans = fs->current_trans;
    
    mutex_lock(&fs->journal_lock);
    mutex_lock(&trans->lock);
    
    serial_print("[ext3_journal] Committing transaction ");
    serial_print_dec(trans->sequence);
    serial_print(" (");
    serial_print_dec(trans->block_count);
    serial_print(" blocks)\n");
    
    /* Write descriptor block */
    uint32_t desc_block = fs->journal_start + 1 + (trans->sequence % (fs->journal_blocks - 2));
    uint8_t *desc_buffer = (uint8_t *)kmalloc(fs->ext2.block_size);
    if (!desc_buffer) {
        mutex_unlock(&trans->lock);
        mutex_unlock(&fs->journal_lock);
        return -1;
    }
    
    memset(desc_buffer, 0, fs->ext2.block_size);
    struct ext3_journal_descriptor *desc = (struct ext3_journal_descriptor *)desc_buffer;
    desc->header.jh_magic = EXT3_JOURNAL_MAGIC;
    desc->header.jh_type = EXT3_JOURNAL_DESCRIPTOR;
    desc->header.jh_sequence = trans->sequence;
    desc->header.jh_blocks = trans->block_count;
    
    ext2_write_block(&fs->ext2, desc_block, desc_buffer);
    kfree(desc_buffer);
    
    /* Write commit block */
    uint32_t commit_block = desc_block + 1;
    if (commit_block >= fs->journal_start + fs->journal_blocks) {
        commit_block = fs->journal_start + 1;
    }
    
    uint8_t *commit_buffer = (uint8_t *)kmalloc(fs->ext2.block_size);
    if (!commit_buffer) {
        mutex_unlock(&trans->lock);
        mutex_unlock(&fs->journal_lock);
        return -1;
    }
    
    memset(commit_buffer, 0, fs->ext2.block_size);
    struct ext3_journal_commit *commit = (struct ext3_journal_commit *)commit_buffer;
    commit->header.jh_magic = EXT3_JOURNAL_MAGIC;
    commit->header.jh_type = EXT3_JOURNAL_COMMIT;
    commit->header.jh_sequence = trans->sequence;
    commit->header.jh_blocks = 0;
    commit->jc_checksum = 0; /* Checksum will be added later */
    
    ext2_write_block(&fs->ext2, commit_block, commit_buffer);
    kfree(commit_buffer);
    
    /* Mark as committed */
    trans->flags |= EXT3_TRANS_COMMITTED;
    fs->journal_state = EXT3_JOURNAL_CLEAN;
    
    /* Update journal superblock */
    ext3_write_journal_sb(fs);
    
    /* Free transaction */
    kfree(trans);
    fs->current_trans = NULL;
    
    mutex_unlock(&trans->lock);
    mutex_unlock(&fs->journal_lock);
    
    serial_print("[ext3_journal] Transaction committed\n");
    return 0;
}

/* Stop current transaction (commit if there are blocks) */
int ext3_journal_stop(struct ext3_filesystem *fs)
{
    if (!fs || !fs->current_trans) return -1;
    
    struct ext3_transaction *trans = fs->current_trans;
    
    if (trans->block_count > 0) {
        return ext3_journal_commit(fs);
    } else {
        /* Empty transaction - just discard */
        mutex_lock(&fs->journal_lock);
        kfree(trans);
        fs->current_trans = NULL;
        mutex_unlock(&fs->journal_lock);
        return 0;
    }
}

/* Get current transaction */
struct ext3_transaction *ext3_journal_get_transaction(void)
{
    return ext3_fs ? ext3_fs->current_trans : NULL;
}

/* Set filesystem pointer */
void ext3_journal_set_fs(struct ext3_filesystem *fs)
{
    ext3_fs = fs;
}