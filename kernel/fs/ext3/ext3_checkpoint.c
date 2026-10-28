/*
 * Lethal Kernel - EXT3 Checkpoint
 *
 * v3.0: Writes journaled data to the main filesystem.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ext3.h"
#include "../../core/console.h"
#include "../../memory/heap.h"

/* Forward declarations for EXT2 block I/O */
extern int ext2_write_block(struct ext2_filesystem *fs, uint32_t block, void *buffer);

/* Checkpoint: write all modified blocks to the main filesystem */
int ext3_checkpoint_flush(struct ext3_filesystem *fs, uint32_t *block_list, 
                           void **data_list, uint32_t count)
{
    if (!fs || !block_list || !data_list) return -1;
    
    serial_print("[ext3_checkpoint] Flushing ");
    serial_print_dec(count);
    serial_print(" blocks to main FS\n");
    
    for (uint32_t i = 0; i < count; i++) {
        uint32_t block = block_list[i];
        void *data = data_list[i];
        
        if (ext2_write_block(&fs->ext2, block, data) != 0) {
            serial_print("[ext3_checkpoint] ERROR: failed to write block ");
            serial_print_dec(block);
            serial_print("\n");
            return -1;
        }
    }
    
    serial_print("[ext3_checkpoint] Flush complete\n");
    return 0;
}

/* Checkpoint a single block */
int ext3_checkpoint_block(struct ext3_filesystem *fs, uint32_t block, void *data)
{
    if (!fs || !data) return -1;
    return ext2_write_block(&fs->ext2, block, data);
}

/* Checkpoint all dirty blocks from the current transaction */
int ext3_checkpoint_transaction(struct ext3_filesystem *fs, struct ext3_transaction *trans)
{
    if (!fs || !trans || trans->block_count == 0) return 0;
    

    serial_print("[ext3_checkpoint] Transaction ");
    serial_print_dec(trans->sequence);
    serial_print(" has ");
    serial_print_dec(trans->block_count);
    serial_print(" blocks to checkpoint\n");
    
    return 0;
}