#include "ata.h"
#include "../../../arch/x86_64/io.h"
#include "../../core/console.h"
#define ATA_REG_DATA 0
#define ATA_REG_ERROR 1
#define ATA_REG_FEATURES 1
#define ATA_REG_SECCOUNT 2
#define ATA_REG_LBA0 3
#define ATA_REG_LBA1 4
#define ATA_REG_LBA2 5
#define ATA_REG_HDDEVSEL 6
#define ATA_REG_COMMAND 7
#define ATA_REG_STATUS 7
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_READ 0x20
#define ATA_CMD_WRITE 0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_SR_BSY 0x80
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01
static ata_device_t devs[ATA_MAX_DEVICES]; static uint32_t count;
static uint8_t status(uint16_t b){return inb(b+ATA_REG_STATUS);} static int wait_not_busy(uint16_t b){for(uint32_t i=0;i<1000000;i++){uint8_t s=status(b);if(!(s&ATA_SR_BSY))return (s&ATA_SR_ERR)?-1:0;}return -1;}
static int identify(uint16_t io,uint16_t ctrl,uint8_t drive,ata_device_t*d){outb(io+ATA_REG_HDDEVSEL,(uint8_t)(0xA0|(drive<<4)));io_wait();outb(io+ATA_REG_SECCOUNT,0);outb(io+ATA_REG_LBA0,0);outb(io+ATA_REG_LBA1,0);outb(io+ATA_REG_LBA2,0);outb(io+ATA_REG_COMMAND,ATA_CMD_IDENTIFY);uint8_t s=status(io);if(s==0)return 0;for(uint32_t i=0;i<1000000;i++){s=status(io);if(s&ATA_SR_ERR)return 0;if((s&ATA_SR_DRQ)&&!(s&ATA_SR_BSY))break;if(i==999999)return -1;}uint16_t id[256];for(int i=0;i<256;i++)id[i]=inw(io+ATA_REG_DATA);d->io_base=io;d->ctrl_base=ctrl;d->drive=drive;d->present=1;d->lba28=(id[49]&0x0200)!=0;d->sectors=(uint64_t)id[60]|((uint64_t)id[61]<<16);for(int i=0;i<40;i+=2){d->model[i]=(char)(id[27+i/2]>>8);d->model[i+1]=(char)(id[27+i/2]&0xFF);}d->model[40]=0;return 1;}
void ata_init(void){count=0;for(int c=0;c<2;c++){uint16_t io=c?0x170:0x1F0,ctrl=c?0x376:0x3F6;for(int drive=0;drive<2;drive++){if(count>=ATA_MAX_DEVICES)break;ata_device_t tmp={0};int r=identify(io,ctrl,(uint8_t)drive,&tmp);if(r==1){devs[count++]=tmp;}}}serial_print("[ata] ATA PIO devices detected: ");serial_print_dec(count);serial_print("\n");for(uint32_t i=0;i<count;i++){serial_print("[ata] device ");serial_print_dec(i);serial_print(" model=");serial_print(devs[i].model);serial_print(" sectors=");serial_print_dec(devs[i].sectors);serial_print("\n");}}
uint32_t ata_device_count(void){return count;} const ata_device_t*ata_get_device(uint32_t i){return i<count?&devs[i]:0;}
static int rw(uint32_t idx,uint32_t lba,void*buf,int write){if(idx>=count||!buf||lba>0x0FFFFFFF)return -1;ata_device_t*d=&devs[idx];uint16_t io=d->io_base;outb(io+ATA_REG_HDDEVSEL,(uint8_t)(0xE0|(d->drive<<4)|((lba>>24)&0x0F)));outb(io+ATA_REG_SECCOUNT,1);outb(io+ATA_REG_LBA0,(uint8_t)lba);outb(io+ATA_REG_LBA1,(uint8_t)(lba>>8));outb(io+ATA_REG_LBA2,(uint8_t)(lba>>16));outb(io+ATA_REG_COMMAND,(uint8_t)(write?ATA_CMD_WRITE:ATA_CMD_READ));if(wait_not_busy(io)<0)return -1;for(uint32_t i=0;i<1000000;i++){uint8_t s=status(io);if(s&ATA_SR_ERR)return -1;if((s&ATA_SR_DRQ)&&!(s&ATA_SR_BSY))break;if(i==999999)return -1;}if(write){const uint16_t*w=(const uint16_t*)buf;for(int i=0;i<256;i++)outw(io+ATA_REG_DATA,w[i]);outb(io+ATA_REG_COMMAND,ATA_CMD_CACHE_FLUSH);if(wait_not_busy(io)<0)return -1;}else{uint16_t*w=(uint16_t*)buf;for(int i=0;i<256;i++)w[i]=inw(io+ATA_REG_DATA);}return 0;}
int ata_read28(uint32_t i,uint32_t lba,void*b){return rw(i,lba,b,0);} int ata_write28(uint32_t i,uint32_t lba,const void*b){return rw(i,lba,(void*)b,1);}
