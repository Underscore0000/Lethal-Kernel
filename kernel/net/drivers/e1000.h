#ifndef LETHAL_NET_DRIVERS_E1000_H
#define LETHAL_NET_DRIVERS_E1000_H

#include <stdint.h>
#include "../core/net.h"
#include "../core/sk_buff.h"

#define E1000_VENDOR_ID 0x8086

#define E1000_DEVICE_ID_100E 0x100E
#define E1000_DEVICE_ID_100F 0x100F
#define E1000_DEVICE_ID_109A 0x109A
#define E1000_DEVICE_ID_10D3 0x10D3
#define E1000_DEVICE_ID_105E 0x105E

#define E1000_CTRL       0x0000
#define E1000_CTRL_FD    (1U << 0)
#define E1000_CTRL_ASDE  (1U << 5)
#define E1000_CTRL_SLU   (1U << 6)
#define E1000_CTRL_ILOS  (1U << 7)
#define E1000_CTRL_SPD   (3U << 8)
#define E1000_CTRL_FRCSPD (1U << 11)
#define E1000_CTRL_FRCDPLX (1U << 12)
#define E1000_CTRL_RST   (1U << 26)
#define E1000_CTRL_VME   (1U << 30)

#define E1000_STATUS     0x0008
#define E1000_STATUS_LU  (1U << 1)

#define E1000_EECD       0x0010
#define E1000_EERD       0x0014

#define E1000_CTRL_EXT   0x0018

#define E1000_ICR        0x00C0
#define E1000_IMS        0x00D0
#define E1000_IMC        0x00D8

#define E1000_RCTL       0x0100
#define E1000_RCTL_EN    (1U << 1)
#define E1000_RCTL_SBP   (1U << 2)
#define E1000_RCTL_UPE   (1U << 3)
#define E1000_RCTL_MPE   (1U << 4)
#define E1000_RCTL_LPE   (1U << 5)
#define E1000_RCTL_BAM   (1U << 15)
#define E1000_RCTL_BSIZE (3U << 16)
#define E1000_RCTL_BSIZE_2048 (0U << 16)
#define E1000_RCTL_SECRC (1U << 26)

#define E1000_TCTL       0x0400
#define E1000_TCTL_EN    (1U << 1)
#define E1000_TCTL_PSP   (1U << 3)

#define E1000_RDBAL      0x2800
#define E1000_RDBAH      0x2804
#define E1000_RDLEN      0x2808
#define E1000_RDH        0x2810
#define E1000_RDT        0x2818

#define E1000_TDBAL      0x3800
#define E1000_TDBAH      0x3804
#define E1000_TDLEN      0x3808
#define E1000_TDH        0x3810
#define E1000_TDT        0x3818

#define E1000_DESC_COUNT 32
#define E1000_BUFFER_SIZE 2048
#define E1000_MTU 1500

struct e1000_rx_desc {
    uint64_t buffer_addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

#define E1000_RXD_STATUS_DD  (1U << 0)
#define E1000_RXD_STATUS_EOP (1U << 1)

struct e1000_tx_desc {
    uint64_t buffer_addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed));

#define E1000_TXD_STATUS_DD  (1U << 0)

#define E1000_TXD_CMD_EOP  (1U << 0)
#define E1000_TXD_CMD_IFCS (1U << 1)
#define E1000_TXD_CMD_IC   (1U << 2)
#define E1000_TXD_CMD_RS   (1U << 3)

struct e1000_device {
    uint32_t io_base;

    /*
     * PCI BAR physical address.
     */
    uint64_t mem_base;

    uint32_t irq;
    uint32_t bar_type;

    uint8_t mac[6];

    uint8_t initialized;
    uint8_t link_up;

    struct e1000_rx_desc rx_desc[E1000_DESC_COUNT]
        __attribute__((aligned(16)));

    struct e1000_tx_desc tx_desc[E1000_DESC_COUNT]
        __attribute__((aligned(16)));

    uint8_t rx_buffer[E1000_DESC_COUNT][E1000_BUFFER_SIZE]
        __attribute__((aligned(16)));

    uint8_t tx_buffer[E1000_DESC_COUNT][E1000_BUFFER_SIZE]
        __attribute__((aligned(16)));

    uint16_t rx_head;
    uint16_t rx_tail;

    uint16_t tx_head;
    uint16_t tx_tail;

    uint64_t rx_packets;
    uint64_t tx_packets;

    uint64_t rx_bytes;
    uint64_t tx_bytes;

    uint64_t rx_errors;
    uint64_t tx_errors;
};

int e1000_init(struct e1000_device *dev);
int e1000_open(struct e1000_device *dev);
int e1000_close(struct e1000_device *dev);

int e1000_send(
    struct e1000_device *dev,
    struct sk_buff *skb
);

int e1000_receive(
    struct e1000_device *dev,
    struct sk_buff **skb
);

void e1000_irq_handler(void *data);

void e1000_driver_init(void);

#endif
