#!/usr/bin/env python3
# tools/mkext2.py

import sys
import struct
import os

def create_ext2_image(path, size_mb=8):
    """Create a minimal EXT2 filesystem image"""
    
    
    block_size = 1024
    blocks = size_mb * 1024 * 1024 // block_size
    inodes = blocks // 4
    
    
    sb = struct.pack('<I I I I I I I I I I I I I I H H H H I I I I H H H H H H I I I I I I I I I I I I I I I I I I I I I I I I I I I I I I I I I',
        1,  # s_inodes_count
        0,  # s_blocks_count
        0,  # s_r_blocks_count
        0,  # s_free_blocks_count
        0,  # s_free_inodes_count
        1,  # s_first_data_block
        0,  # s_log_block_size
        0,  # s_log_frag_size
        0,  # s_blocks_per_group
        0,  # s_frags_per_group
        0,  # s_inodes_per_group
        0,  # s_mtime
        0,  # s_wtime
        0,  # s_mnt_count
        0,  # s_max_mnt_count
        0xEF53,  # s_magic
        1,  # s_state
        1,  # s_errors
        0,  # s_minor_rev_level
        0,  # s_lastcheck
        0,  # s_checkinterval
        0,  # s_creator_os
        0,  # s_rev_level
        0,  # s_def_resuid
        0,  # s_def_resgid
        11,  # s_first_ino
        128,  # s_inode_size
        0,  # s_block_group_nr
        0,  # s_feature_compat
        0,  # s_feature_incompat
        0,  # s_feature_ro_compat
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  # s_uuid
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  # s_volume_name
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  # s_last_mounted
    )
    
    # Crea file vuoto
    with open(path, 'wb') as f:
        f.truncate(size_mb * 1024 * 1024)
        f.seek(1024)
        f.write(sb)
    
    print(f"Created EXT2 image: {path} ({size_mb} MiB)")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: mkext2.py <image_path> [size_mb]")
        sys.exit(1)
    size = int(sys.argv[2]) if len(sys.argv) > 2 else 8
    create_ext2_image(sys.argv[1], size)