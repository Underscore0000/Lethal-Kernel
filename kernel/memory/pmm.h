#ifndef LETHAL_PMM_H
#define LETHAL_PMM_H

#include <stdint.h>
#include <stddef.h>
#include "../core/multiboot.h"

#define PMM_FRAME_SIZE 4096

void pmm_init(const struct e820_entry *map, uint16_t entry_count,
              uint64_t kernel_start, uint64_t kernel_end);
uint64_t pmm_alloc_frame(void);
void pmm_free_frame(uint64_t phys_addr);

/* v0.11 reference counting: a frame is returned to the free bitmap only
 * when its reference count reaches zero. This is used by copy-on-write. */
void pmm_ref_frame(uint64_t phys_addr);
uint32_t pmm_frame_refs(uint64_t phys_addr);

uint64_t pmm_total_bytes(void);
uint64_t pmm_free_bytes(void);
uint64_t pmm_used_bytes(void);

#endif
