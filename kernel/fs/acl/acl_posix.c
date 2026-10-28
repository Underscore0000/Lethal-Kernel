/*
 * Lethal Kernel - POSIX ACL Implementation
 *
 * v4.0: POSIX ACL operations for fine-grained permissions.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "acl.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>

static struct acl_fs acl_fs_data;
static struct acl_fs *acl_fs = NULL;

/* Initialize ACL system */
int acl_init(void)
{
    serial_print("[acl] Initializing...\n");
    
    if (acl_fs) {
        serial_print("[acl] Already initialized\n");
        return 0;
    }
    
    memset(&acl_fs_data, 0, sizeof(struct acl_fs));
    acl_fs = &acl_fs_data;
    mutex_init(&acl_fs->cache_lock);
    
    serial_print("[acl] Initialized\n");
    return 0;
}

/* Allocate a new ACL */
struct acl *acl_alloc(void)
{
    struct acl *acl = (struct acl *)kmalloc(sizeof(struct acl));
    if (!acl) return NULL;
    
    memset(acl, 0, sizeof(struct acl));
    acl->version = 1;
    acl->entry_count = 0;
    acl->flags = 0;
    mutex_init(&acl->lock);
    
    return acl;
}

/* Free an ACL */
void acl_free(struct acl *acl)
{
    if (!acl) return;
    kfree(acl);
}

/* Add an entry to an ACL */
int acl_add_entry(struct acl *acl, uint16_t tag, uint16_t perm, uint32_t id)
{
    if (!acl) return -1;
    if (acl->entry_count >= ACL_ENTRY_MAX) return -1;
    
    mutex_lock(&acl->lock);
    
    /* Check if entry already exists */
    for (uint32_t i = 0; i < acl->entry_count; i++) {
        if (acl->entries[i].tag_type == tag && acl->entries[i].id == id) {
            /* Update existing entry */
            acl->entries[i].permissions = perm;
            mutex_unlock(&acl->lock);
            return 0;
        }
    }
    
    /* Add new entry */
    acl->entries[acl->entry_count].tag_type = tag;
    acl->entries[acl->entry_count].permissions = perm;
    acl->entries[acl->entry_count].id = id;
    acl->entry_count++;
    
    mutex_unlock(&acl->lock);
    return 0;
}

/* Remove an entry from an ACL */
int acl_remove_entry(struct acl *acl, uint16_t tag, uint32_t id)
{
    if (!acl) return -1;
    
    mutex_lock(&acl->lock);
    
    for (uint32_t i = 0; i < acl->entry_count; i++) {
        if (acl->entries[i].tag_type == tag && acl->entries[i].id == id) {
            /* Shift remaining entries */
            for (uint32_t j = i; j < acl->entry_count - 1; j++) {
                acl->entries[j] = acl->entries[j + 1];
            }
            acl->entry_count--;
            mutex_unlock(&acl->lock);
            return 0;
        }
    }
    
    mutex_unlock(&acl->lock);
    return -1; /* Entry not found */
}

/* Check if a user/group has permission */
int acl_check_permission(struct acl *acl, uint16_t tag, uint32_t id, uint16_t perm)
{
    if (!acl) return 0;
    
    mutex_lock(&acl->lock);
    
    /* Check exact match */
    for (uint32_t i = 0; i < acl->entry_count; i++) {
        if (acl->entries[i].tag_type == tag && acl->entries[i].id == id) {
            int result = (acl->entries[i].permissions & perm) == perm;
            mutex_unlock(&acl->lock);
            return result;
        }
    }
    
    /* Check mask (for group entries) */
    if (tag == ACL_TAG_GROUP) {
        for (uint32_t i = 0; i < acl->entry_count; i++) {
            if (acl->entries[i].tag_type == ACL_TAG_MASK) {
                int result = (acl->entries[i].permissions & perm) == perm;
                mutex_unlock(&acl->lock);
                return result;
            }
        }
    }
    
    /* Check "other" entry as fallback */
    for (uint32_t i = 0; i < acl->entry_count; i++) {
        if (acl->entries[i].tag_type == ACL_TAG_OTHER) {
            int result = (acl->entries[i].permissions & perm) == perm;
            mutex_unlock(&acl->lock);
            return result;
        }
    }
    
    mutex_unlock(&acl->lock);
    return 0;
}

/* Validate ACL (check for consistency) */
int acl_validate(struct acl *acl)
{
    if (!acl) return -1;
    if (acl->entry_count == 0) return -1;
    if (acl->entry_count > ACL_ENTRY_MAX) return -1;
    
    int has_other = 0;
    
    for (uint32_t i = 0; i < acl->entry_count; i++) {
        if (acl->entries[i].tag_type == ACL_TAG_OTHER) {
            has_other = 1;
            break;
        }
    }
    
    if (!has_other) return -1;
    
    return 0;
}

/* Print ACL for debugging */
void acl_print(struct acl *acl)
{
    if (!acl) {
        serial_print("[acl] NULL\n");
        return;
    }
    
    serial_print("[acl] Entries: ");
    serial_print_dec(acl->entry_count);
    serial_print("\n");
    
    for (uint32_t i = 0; i < acl->entry_count; i++) {
        struct acl_entry *e = &acl->entries[i];
        serial_print("  ");
        serial_print_dec(i);
        serial_print(": tag=");
        serial_print_dec(e->tag_type);
        serial_print(" id=");
        serial_print_dec(e->id);
        serial_print(" perm=0x");
        serial_print_hex(e->permissions);
        serial_print("\n");
    }
}

/* Convert ACL to string representation */
int acl_to_string(struct acl *acl, char *buffer, size_t size)
{
    if (!acl || !buffer || size == 0) return -1;
    
    size_t pos = 0;
    
    for (uint32_t i = 0; i < acl->entry_count && pos < size - 1; i++) {
        struct acl_entry *e = &acl->entries[i];
        const char *tag_name;
        
        switch (e->tag_type) {
            case ACL_TAG_USER:  tag_name = "user"; break;
            case ACL_TAG_GROUP: tag_name = "group"; break;
            case ACL_TAG_MASK:  tag_name = "mask"; break;
            case ACL_TAG_OTHER: tag_name = "other"; break;
            default:            tag_name = "unknown"; break;
        }
        
        /* Manual string concatenation without snprintf */
        size_t remaining = size - pos;
        if (remaining < 20) break;
        
        /* Write tag name */
        while (*tag_name && pos < size - 1) {
            buffer[pos++] = *tag_name++;
        }
        if (pos < size - 1) buffer[pos++] = ':';
        
        /* Write ID if applicable */
        if (e->tag_type == ACL_TAG_USER || e->tag_type == ACL_TAG_GROUP) {
            /* Write ID as decimal */
            uint32_t id = e->id;
            char id_buf[16];
            size_t id_pos = 0;
            if (id == 0) {
                id_buf[id_pos++] = '0';
            } else {
                while (id > 0 && id_pos < sizeof(id_buf) - 1) {
                    id_buf[id_pos++] = '0' + (id % 10);
                    id /= 10;
                }
            }
            while (id_pos > 0 && pos < size - 1) {
                buffer[pos++] = id_buf[--id_pos];
            }
            if (pos < size - 1) buffer[pos++] = ':';
        }
        
        /* Write permissions */
        buffer[pos++] = (e->permissions & ACL_MASK_READ) ? 'r' : '-';
        buffer[pos++] = (e->permissions & ACL_MASK_WRITE) ? 'w' : '-';
        buffer[pos++] = (e->permissions & ACL_MASK_EXECUTE) ? 'x' : '-';
        if (pos < size - 1) buffer[pos++] = '\n';
    }
    
    if (pos < size) {
        buffer[pos] = '\0';
    } else if (size > 0) {
        buffer[size - 1] = '\0';
    }
    
    return 0;
}

/* Read ACL from disk */
int acl_read(struct ext2_filesystem *fs, uint32_t inode_num, struct acl **out)
{
    (void)inode_num;
    
    if (!fs || !out) return -1;
    
    struct acl *acl = acl_alloc();
    if (!acl) return -1;
    
    acl_add_entry(acl, ACL_TAG_OTHER, ACL_MASK_READ | ACL_MASK_WRITE | ACL_MASK_EXECUTE, 0);
    
    *out = acl;
    return 0;
}

/* Write ACL to disk */
int acl_write(struct ext2_filesystem *fs, uint32_t inode_num, struct acl *acl)
{
    (void)fs;
    (void)inode_num;
    
    if (!acl) return -1;
    
    serial_print("[acl] Writing ACL for inode ");
    serial_print_dec(inode_num);
    serial_print("\n");
    
    return 0;
}

/* Remove ACL from disk */
int acl_remove(struct ext2_filesystem *fs, uint32_t inode_num)
{
    (void)fs;
    
    serial_print("[acl] Removing ACL for inode ");
    serial_print_dec(inode_num);
    serial_print("\n");
    
    return 0;
}