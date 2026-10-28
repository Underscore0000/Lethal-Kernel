/*
 * Lethal Kernel - EXT2 Permissions
 *
 * v2.1: User/Group IDs and file permissions.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ext2.h"
#include "../../core/console.h"
#include <string.h>

/* Check if a user has permission to access a file */
int ext2_check_permission(struct ext2_inode *inode, uint16_t uid, uint16_t gid, int mode)
{
    uint16_t file_mode = inode->i_mode;
    
    /* Root has all permissions */
    if (uid == 0) return 1;
    
    /* Owner permissions */
    if (uid == inode->i_uid) {
        if (mode & 0x01) return (file_mode & EXT2_S_IXUSR) != 0;  /* Execute */
        if (mode & 0x02) return (file_mode & EXT2_S_IWUSR) != 0;  /* Write */
        return (file_mode & EXT2_S_IRUSR) != 0;                    /* Read */
    }
    
    /* Group permissions */
    if (gid == inode->i_gid) {
        if (mode & 0x01) return (file_mode & EXT2_S_IXGRP) != 0;
        if (mode & 0x02) return (file_mode & EXT2_S_IWGRP) != 0;
        return (file_mode & EXT2_S_IRGRP) != 0;
    }
    
    /* Others permissions */
    if (mode & 0x01) return (file_mode & EXT2_S_IXOTH) != 0;
    if (mode & 0x02) return (file_mode & EXT2_S_IWOTH) != 0;
    return (file_mode & EXT2_S_IROTH) != 0;
}

/* Set file permissions */
int ext2_set_permission(struct ext2_filesystem *fs, uint32_t inode_num, uint16_t mode)
{
    struct ext2_inode inode;
    if (ext2_read_inode(fs, inode_num, &inode) != 0) {
        return -1;
    }
    
    inode.i_mode = (inode.i_mode & 0xF000) | (mode & 0x0FFF);
    return ext2_write_inode(fs, inode_num, &inode);
}

/* Set owner */
int ext2_set_owner(struct ext2_filesystem *fs, uint32_t inode_num, uint16_t uid, uint16_t gid)
{
    struct ext2_inode inode;
    if (ext2_read_inode(fs, inode_num, &inode) != 0) {
        return -1;
    }
    
    inode.i_uid = uid;
    inode.i_gid = gid;
    return ext2_write_inode(fs, inode_num, &inode);
}

/* Get file mode string for debugging */
void ext2_mode_to_string(uint16_t mode, char *buffer, size_t size)
{
    char type_char;
    
    if (mode & EXT2_S_IFDIR) {
        type_char = 'd';
    } else if (mode & EXT2_S_IFLNK) {
        type_char = 'l';
    } else if (mode & EXT2_S_IFREG) {
        type_char = '-';
    } else if (mode & EXT2_S_IFBLK) {
        type_char = 'b';
    } else if (mode & EXT2_S_IFCHR) {
        type_char = 'c';
    } else if (mode & EXT2_S_IFIFO) {
        type_char = 'p';
    } else if (mode & EXT2_S_IFSOCK) {
        type_char = 's';
    } else {
        type_char = '?';
    }
    
    char perm_str[10];
    perm_str[0] = (mode & EXT2_S_IRUSR) ? 'r' : '-';
    perm_str[1] = (mode & EXT2_S_IWUSR) ? 'w' : '-';
    perm_str[2] = (mode & EXT2_S_IXUSR) ? 'x' : '-';
    perm_str[3] = (mode & EXT2_S_IRGRP) ? 'r' : '-';
    perm_str[4] = (mode & EXT2_S_IWGRP) ? 'w' : '-';
    perm_str[5] = (mode & EXT2_S_IXGRP) ? 'x' : '-';
    perm_str[6] = (mode & EXT2_S_IROTH) ? 'r' : '-';
    perm_str[7] = (mode & EXT2_S_IWOTH) ? 'w' : '-';
    perm_str[8] = (mode & EXT2_S_IXOTH) ? 'x' : '-';
    perm_str[9] = '\0';
    
    /* Costruisci la stringa nel buffer */
    size_t i = 0;
    if (size > 0) {
        buffer[i++] = type_char;
        for (int j = 0; j < 9 && i < size - 1; j++) {
            buffer[i++] = perm_str[j];
        }
        buffer[i] = '\0';
    }
}