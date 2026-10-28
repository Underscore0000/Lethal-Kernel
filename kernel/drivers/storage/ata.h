#ifndef LETHAL_ATA_H
#define LETHAL_ATA_H
#include <stdint.h>
#define ATA_SECTOR_SIZE 512
#define ATA_MAX_DEVICES 4
typedef struct { uint16_t io_base; uint16_t ctrl_base; uint8_t drive; uint8_t present; uint8_t lba28; uint64_t sectors; char model[41]; } ata_device_t;
void ata_init(void); uint32_t ata_device_count(void); const ata_device_t *ata_get_device(uint32_t index);
int ata_read28(uint32_t index,uint32_t lba,void *buffer); int ata_write28(uint32_t index,uint32_t lba,const void *buffer);
#endif
