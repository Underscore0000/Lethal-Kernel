#ifndef LETHAL_FAT16_H
#define LETHAL_FAT16_H

#include <stddef.h>
#include <stdint.h>
#include "vfs.h"

#define FAT16_EOC 0xFFF8u
#define FAT16_BAD 0xFFF7u

int fat16_mount(uint32_t ata_device);
int fat16_open(const char *path, uint32_t flags, void **out);
long fat16_read(void *file, void *buffer, size_t count);
long fat16_write(void *file, const void *buffer, size_t count);
int fat16_close(void *file);
long fat16_size(void *file);

/* VFS ops - defined in fat16.c */
extern struct vfs_filesystem_ops fat16_ops;

#endif