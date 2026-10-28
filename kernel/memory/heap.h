#ifndef LETHAL_HEAP_H
#define LETHAL_HEAP_H

#include <stddef.h>
#include <stdint.h>

/* Initializes the kernel heap starting at `virt_start`, growing up to
 * `max_size` bytes by mapping fresh PMM frames via the VMM as needed. */
void heap_init(uint64_t virt_start, uint64_t max_size);

void *kmalloc(size_t size);
void  kfree(void *ptr);
void *kcalloc(size_t nmemb, size_t size);
void *krealloc(void *ptr, size_t size);

uint64_t heap_used_bytes(void);
uint64_t heap_total_bytes(void);


size_t heap_block_size(const void *p);

void *heap_find_used_block(size_t min_size);

#endif