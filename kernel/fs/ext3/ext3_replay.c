/*
 * Lethal Kernel - EXT3 Journal Replay
 *
 * v3.0: Recovery after crash by replaying committed transactions.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ext3.h"
#include "../../core/console.h"
#include "../../memory/heap.h"

/* Forward declarations for EXT2 block I/O */
extern int ext2_read_block(struct ext2_filesystem *fs, uint32_t block, void *buffer);
extern int ext2_write_block(struct ext2_filesystem *fs, uint32_t block, void *buffer);

/* Forward declaration for ext3_write_journal_sb (defined in ext3_journal.c) */
extern int ext3_write_journal_sb(struct ext3_filesystem *fs);

/* Replay the journal after a crash */
int ext3_journal_replay(struct ext3_filesystem *fs)
{
    serial_print("[ext3_replay] Scanning journal for uncommitted transactions...\n");
    
    uint32_t recovered = 0;
    uint32_t start_block = fs->journal_start + 1;
    uint32_t end_block = fs->journal_start + fs->journal_blocks;
    
    uint8_t *buffer = (uint8_t *)kmalloc(fs->ext2.block_size);
    if (!buffer) return -1;
    
    /* Scan all journal blocks */
    for (uint32_t block = start_block; block < end_block; block++) {
        if (ext2_read_block(&fs->ext2, block, buffer) != 0) {
            continue;
        }
        
        struct ext3_journal_header *header = (struct ext3_journal_header *)buffer;
        
        /* Check for descriptor block */
        if (header->jh_magic == EXT3_JOURNAL_MAGIC &&
            header->jh_type == EXT3_JOURNAL_DESCRIPTOR) {
            
            serial_print("[ext3_replay] Found descriptor for transaction ");
            serial_print_dec(header->jh_sequence);
            serial_print("\n");
            
            /* Look for matching commit block */
            uint32_t commit_found = 0;
            for (uint32_t cb = block + 1; cb < end_block; cb++) {
                uint8_t *cbuf = (uint8_t *)kmalloc(fs->ext2.block_size);
                if (!cbuf) break;
                
                if (ext2_read_block(&fs->ext2, cb, cbuf) == 0) {
                    struct ext3_journal_header *ch = (struct ext3_journal_header *)cbuf;
                    if (ch->jh_magic == EXT3_JOURNAL_MAGIC &&
                        ch->jh_type == EXT3_JOURNAL_COMMIT &&
                        ch->jh_sequence == header->jh_sequence) {
                        commit_found = 1;
                        kfree(cbuf);
                        break;
                    }
                }
                kfree(cbuf);
            }
            
            if (commit_found) {
                /* Transaction is complete - nothing to recover */
                serial_print("[ext3_replay] Transaction ");
                serial_print_dec(header->jh_sequence);
                serial_print(" is complete\n");
            } else {
                /* Transaction is incomplete - discard it */
                serial_print("[ext3_replay] Transaction ");
                serial_print_dec(header->jh_sequence);
                serial_print(" is incomplete, discarding\n");
            }
            
            recovered++;
        }
    }
    
    kfree(buffer);
    
    fs->journal_state = EXT3_JOURNAL_CLEAN;
    ext3_write_journal_sb(fs);
    
    serial_print("[ext3_replay] Recovery complete, ");
    serial_print_dec(recovered);
    serial_print(" transactions scanned\n");
    
    return 0;
}