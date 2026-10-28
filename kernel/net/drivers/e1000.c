/*
 * Lethal Kernel - Intel e1000 NIC Driver
 *
 * v5.0: Intel e1000 network driver.
 */

#include "e1000.h"

#include "../core/device.h"
#include "../core/net.h"
#include "../core/sk_buff.h"

#include "../../core/console.h"
#include "../../core/irq.h"
#include "../../memory/pmm.h"
#include "../../pci/pci.h"
#include "../../memory/vmm.h"

#include <string.h>

static struct e1000_device e1000_devices[4];
static uint32_t e1000_device_count = 0;

/* ============================================================================
 * Register Access
 * ============================================================================ */

static uint32_t e1000_read_reg(
    struct e1000_device *dev,
    uint32_t reg)
{
    volatile uint32_t *addr;

    if (!dev)
        return 0;

    addr = (volatile uint32_t *)(
        uintptr_t)(dev->mem_base + reg);

    return *addr;
}

static void e1000_write_reg(
    struct e1000_device *dev,
    uint32_t reg,
    uint32_t value)
{
    volatile uint32_t *addr;

    if (!dev)
        return;

    addr = (volatile uint32_t *)(
        uintptr_t)(dev->mem_base + reg);

    *addr = value;
}

/* ============================================================================
 * EEPROM
 * ============================================================================ */

static uint16_t e1000_read_eeprom(
    struct e1000_device *dev,
    uint8_t address)
{
    uint32_t value = 0;

    e1000_write_reg(
        dev,
        E1000_EERD,
        ((uint32_t)address << 8) | 1U
    );

    for (uint32_t i = 0; i < 100000; i++) {
        value = e1000_read_reg(dev, E1000_EERD);

        if (value & (1U << 4))
            return (uint16_t)(value >> 16);
    }

    return 0;
}

/* ============================================================================
 * MAC
 * ============================================================================ */

static void e1000_read_mac(
    struct e1000_device *dev)
{
    int all_zero = 1;
    
    for (int i = 0; i < 3; i++) {
        uint16_t word;

        word = e1000_read_eeprom(
            dev,
            (uint8_t)i
        );

        dev->mac[i * 2] =
            (uint8_t)(word & 0xFF);

        dev->mac[i * 2 + 1] =
            (uint8_t)((word >> 8) & 0xFF);
    }


    for (int i = 0; i < 6; i++) {
        if (dev->mac[i] != 0) {
            all_zero = 0;
            break;
        }
    }


    if (all_zero) {

        dev->mac[0] = 0x52;
        dev->mac[1] = 0x54;
        dev->mac[2] = 0x00;
        dev->mac[3] = 0x12;
        dev->mac[4] = 0x34;
        dev->mac[5] = 0x56;
    }
}

/* ============================================================================
 * Reset
 * ============================================================================ */

static int e1000_reset(
    struct e1000_device *dev)
{
    uint32_t ctrl;

    if (!dev)
        return -1;

    ctrl = e1000_read_reg(
        dev,
        E1000_CTRL
    );

    ctrl |= E1000_CTRL_RST;

    e1000_write_reg(
        dev,
        E1000_CTRL,
        ctrl
    );

    for (uint32_t i = 0; i < 100000; i++) {
        ctrl = e1000_read_reg(
            dev,
            E1000_CTRL
        );

        if (!(ctrl & E1000_CTRL_RST))
            return 0;
    }

    serial_print(
        "[e1000] WARNING: reset timeout\n"
    );

    return -1;
}

/* ============================================================================
 * Device Initialization
 * ============================================================================ */

int e1000_init(
    struct e1000_device *dev)
{
    uint32_t rctl;
    uint32_t tctl;

    if (!dev)
        return -1;

    serial_print(
        "[e1000] Initializing device...\n"
    );

    serial_print("[e1000] MMIO base: 0x");
    serial_print_hex(dev->mem_base);
    serial_print("\n");

    /*
     * BAR0 must already be mapped by the PCI driver.
     */
    if (vmm_get_phys(dev->mem_base) == 0) {
        serial_print(
            "[e1000] ERROR: MMIO BAR is not mapped\n"
        );
        return -1;
    }

    if (e1000_reset(dev) != 0)
        return -1;

    /*
     * Establish full duplex + link.
     */
    uint32_t ctrl = e1000_read_reg(
        dev,
        E1000_CTRL
    );

    ctrl |= E1000_CTRL_SLU;
    ctrl |= E1000_CTRL_FD;

    e1000_write_reg(
        dev,
        E1000_CTRL,
        ctrl
    );

    e1000_read_mac(dev);

    serial_print("[e1000] MAC: ");

    for (int i = 0; i < 6; i++) {
        serial_print_hex8(dev->mac[i]);

        if (i != 5)
            serial_print(":");
    }
    serial_print("\n");

    /*
     * RX descriptors.
     *
     * The kernel is identity mapped below 4 GiB during early boot,
     * so these static addresses are also valid DMA physical addresses.
     */
    memset(
        dev->rx_desc,
        0,
        sizeof(dev->rx_desc)
    );

    memset(
        dev->tx_desc,
        0,
        sizeof(dev->tx_desc)
    );

    dev->rx_head = 0;
    dev->rx_tail = E1000_DESC_COUNT - 1;

    dev->tx_head = 0;
    dev->tx_tail = 0;

    for (uint32_t i = 0;
         i < E1000_DESC_COUNT;
         i++) {

        dev->rx_desc[i].buffer_addr =
            (uint64_t)(uintptr_t)
                dev->rx_buffer[i];

        dev->rx_desc[i].status = 0;

        dev->tx_desc[i].buffer_addr =
            (uint64_t)(uintptr_t)
                dev->tx_buffer[i];

        /*
         * DD=1 means available for software.
         */
        dev->tx_desc[i].status =
            E1000_TXD_STATUS_DD;
    }

    /*
     * RX descriptor ring.
     */
    uint64_t rx_phys =
        (uint64_t)(uintptr_t)dev->rx_desc;

    e1000_write_reg(
        dev,
        E1000_RDBAL,
        (uint32_t)(rx_phys & 0xFFFFFFFFULL)
    );

    e1000_write_reg(
        dev,
        E1000_RDBAH,
        (uint32_t)(rx_phys >> 32)
    );

    e1000_write_reg(
        dev,
        E1000_RDLEN,
        sizeof(struct e1000_rx_desc) *
        E1000_DESC_COUNT
    );

    e1000_write_reg(
        dev,
        E1000_RDH,
        0
    );

    e1000_write_reg(
        dev,
        E1000_RDT,
        E1000_DESC_COUNT - 1
    );

    /*
     * TX descriptor ring.
     */
    uint64_t tx_phys =
        (uint64_t)(uintptr_t)dev->tx_desc;

    e1000_write_reg(
        dev,
        E1000_TDBAL,
        (uint32_t)(tx_phys & 0xFFFFFFFFULL)
    );

    e1000_write_reg(
        dev,
        E1000_TDBAH,
        (uint32_t)(tx_phys >> 32)
    );

    e1000_write_reg(
        dev,
        E1000_TDLEN,
        sizeof(struct e1000_tx_desc) *
        E1000_DESC_COUNT
    );

    e1000_write_reg(
        dev,
        E1000_TDH,
        0
    );

    e1000_write_reg(
        dev,
        E1000_TDT,
        0
    );

    /*
     * RX configuration.
     */
    rctl = e1000_read_reg(
        dev,
        E1000_RCTL
    );

    rctl |= E1000_RCTL_EN;
    rctl |= E1000_RCTL_SBP;
    rctl |= E1000_RCTL_UPE;
    rctl |= E1000_RCTL_MPE;
    rctl |= E1000_RCTL_BAM;

    rctl &= ~E1000_RCTL_BSIZE;

    /*
     * 2048-byte receive buffers.
     */
    rctl |= E1000_RCTL_BSIZE_2048;

    /*
     * Strip Ethernet CRC.
     */
    rctl |= E1000_RCTL_SECRC;

    e1000_write_reg(
        dev,
        E1000_RCTL,
        rctl
    );

    /*
     * TX configuration.
     */
    tctl = e1000_read_reg(
        dev,
        E1000_TCTL
    );

    tctl |= E1000_TCTL_EN;
    tctl |= E1000_TCTL_PSP;

    /*
     * Collision threshold.
     */
    tctl &= ~(0xFFU << 4);
    tctl |= (0x0FU << 4);

    /*
     * Collision distance.
     */
    tctl &= ~(0x3FFU << 12);
    tctl |= (0x40U << 12);

    e1000_write_reg(
        dev,
        E1000_TCTL,
        tctl
    );

    /*
     * Clear pending interrupts.
     */
    (void)e1000_read_reg(
        dev,
        E1000_ICR
    );

    /*
     * RX + TX + link status.
     */
    e1000_write_reg(
        dev,
        E1000_IMS,
        0x00000017U
    );

    dev->initialized = 1;

    dev->link_up =
        !!(e1000_read_reg(
            dev,
            E1000_STATUS
        ) & E1000_STATUS_LU);

    /*
     * QEMU normally reports link up shortly after initialization.
     * Do not make driver initialization fail just because link is
     * not currently negotiated.
     */
    serial_print("[e1000] Device initialized\n");

    return 0;
}

/* ============================================================================
 * Open
 * ============================================================================ */

int e1000_open(
    struct e1000_device *dev)
{
    uint32_t rctl;
    uint32_t tctl;

    if (!dev || !dev->initialized)
        return -1;

    e1000_write_reg(
        dev,
        E1000_IMS,
        0x00000017U
    );

    rctl = e1000_read_reg(
        dev,
        E1000_RCTL
    );

    rctl |= E1000_RCTL_EN;

    e1000_write_reg(
        dev,
        E1000_RCTL,
        rctl
    );

    tctl = e1000_read_reg(
        dev,
        E1000_TCTL
    );

    tctl |= E1000_TCTL_EN;

    e1000_write_reg(
        dev,
        E1000_TCTL,
        tctl
    );

    dev->link_up =
        !!(e1000_read_reg(
            dev,
            E1000_STATUS
        ) & E1000_STATUS_LU);

    return 0;
}

/* ============================================================================
 * Close
 * ============================================================================ */

int e1000_close(
    struct e1000_device *dev)
{
    uint32_t rctl;
    uint32_t tctl;

    if (!dev)
        return -1;

    e1000_write_reg(
        dev,
        E1000_IMC,
        0xFFFFFFFFU
    );

    rctl = e1000_read_reg(
        dev,
        E1000_RCTL
    );

    rctl &= ~E1000_RCTL_EN;

    e1000_write_reg(
        dev,
        E1000_RCTL,
        rctl
    );

    tctl = e1000_read_reg(
        dev,
        E1000_TCTL
    );

    tctl &= ~E1000_TCTL_EN;

    e1000_write_reg(
        dev,
        E1000_TCTL,
        tctl
    );

    dev->link_up = 0;

    return 0;
}

/* ============================================================================
 * Send
 * ============================================================================ */

int e1000_send(
    struct e1000_device *dev,
    struct sk_buff *skb)
{
    uint16_t index;
    struct e1000_tx_desc *desc;

    if (!dev ||
        !skb ||
        !dev->initialized ||
        !dev->link_up) {
        return -1;
    }

    if (skb->len == 0 ||
        skb->len > E1000_BUFFER_SIZE) {
        return -1;
    }

    index = dev->tx_head;

    desc = &dev->tx_desc[index];

    /*
     * Hardware owns descriptor unless DD is set.
     */
    if (!(desc->status & E1000_TXD_STATUS_DD))
        return -1;

    memcpy(
        dev->tx_buffer[index],
        skb->data,
        skb->len
    );

    desc->length = (uint16_t)skb->len;
    desc->cso = 0;

    desc->cmd =
        E1000_TXD_CMD_EOP |
        E1000_TXD_CMD_IFCS |
        E1000_TXD_CMD_RS;

    desc->status = 0;

    dev->tx_head =
        (uint16_t)(
            (index + 1) %
            E1000_DESC_COUNT
        );

    e1000_write_reg(
        dev,
        E1000_TDT,
        dev->tx_head
    );

    dev->tx_packets++;
    dev->tx_bytes += skb->len;

    return 0;
}

/* ============================================================================
 * Receive
 * ============================================================================ */

int e1000_receive(
    struct e1000_device *dev,
    struct sk_buff **skb)
{
    uint16_t index;
    uint16_t length;
    struct e1000_rx_desc *desc;

    if (!dev || !skb)
        return -1;

    *skb = NULL;

    index = dev->rx_head;

    desc = &dev->rx_desc[index];

    if (!(desc->status & E1000_RXD_STATUS_DD))
        return -1;

    length = desc->length;

    if (length == 0 ||
        length > E1000_BUFFER_SIZE) {

        dev->rx_errors++;

        desc->status = 0;

        e1000_write_reg(
            dev,
            E1000_RDT,
            index
        );

        dev->rx_head =
            (uint16_t)(
                (index + 1) %
                E1000_DESC_COUNT
            );

        return -1;
    }

    *skb = skb_alloc(length);

    if (!*skb) {
        dev->rx_errors++;

        desc->status = 0;

        e1000_write_reg(
            dev,
            E1000_RDT,
            index
        );

        return -1;
    }

    memcpy(
        (*skb)->data,
        dev->rx_buffer[index],
        length
    );

    (*skb)->len = length;
    (*skb)->protocol = 0;

    desc->status = 0;

    /*
     * Give descriptor back to hardware.
     */
    e1000_write_reg(
        dev,
        E1000_RDT,
        index
    );

    dev->rx_head =
        (uint16_t)(
            (index + 1) %
            E1000_DESC_COUNT
        );

    dev->rx_packets++;
    dev->rx_bytes += length;

    return 0;
}

/* ============================================================================
 * IRQ
 * ============================================================================ */

void e1000_irq_handler(void *data)
{
    struct e1000_device *dev;
    uint32_t icr;

    dev = (struct e1000_device *)data;

    if (!dev || !dev->initialized)
        return;

    icr = e1000_read_reg(
        dev,
        E1000_ICR
    );

    if (!icr)
        return;

    /*
     * RX interrupt.
     */
    if (icr & 0x04U) {
        struct sk_buff *skb;

        while (e1000_receive(
                   dev,
                   &skb) == 0) {

            if (skb)
                (void)net_input(skb);
        }
    }

    /*
     * TX interrupt.
     *
     * DD is written by hardware when transmission
     * has completed.
     */
    if (icr & 0x02U) {
        for (uint32_t i = 0;
             i < E1000_DESC_COUNT;
             i++) {

            if (dev->tx_desc[i].status &
                E1000_TXD_STATUS_DD) {
                /*
                 * Descriptor is available again.
                 */
            }
        }
    }

    /*
     * Link status change.
     */
    if (icr & 0x10U) {
        uint32_t status;

        status = e1000_read_reg(
            dev,
            E1000_STATUS
        );

        dev->link_up =
            !!(status & E1000_STATUS_LU);

        serial_print("[e1000] Link: ");

        if (dev->link_up)
            serial_print("UP\n");
        else
            serial_print("DOWN\n");
    }
}

/* ============================================================================
 * net_device transmit wrapper
 * ============================================================================ */

static int e1000_netdev_transmit(
    struct net_device *netdev,
    struct sk_buff *skb)
{
    struct e1000_device *dev;

    if (!netdev)
        return -1;

    dev = (struct e1000_device *)netdev->priv;

    return e1000_send(
        dev,
        skb
    );
}

/* ============================================================================
 * Driver Discovery
 * ============================================================================ */

void e1000_driver_init(void)
{
    serial_print(
        "[e1000] Driver init...\n"
    );

    for (int bus = 0;
         bus < 256 &&
         e1000_device_count < 4;
         bus++) {

        for (int device = 0;
             device < 32 &&
             e1000_device_count < 4;
             device++) {

            for (int function = 0;
                 function < 8 &&
                 e1000_device_count < 4;
                 function++) {

                pci_device_t pci_dev;

                if (pci_read_device(
                        bus,
                        device,
                        function,
                        &pci_dev) != 0) {
                    continue;
                }

                if (pci_dev.vendor_id !=
                    E1000_VENDOR_ID) {
                    continue;
                }

                if (pci_dev.device_id !=
                        E1000_DEVICE_ID_100E &&
                    pci_dev.device_id !=
                        E1000_DEVICE_ID_100F &&
                    pci_dev.device_id !=
                        E1000_DEVICE_ID_109A &&
                    pci_dev.device_id !=
                        E1000_DEVICE_ID_10D3 &&
                    pci_dev.device_id !=
                        E1000_DEVICE_ID_105E) {
                    continue;
                }

                serial_print(
                    "[e1000] Found Intel e1000 NIC at bus "
                );

                serial_print_dec(
                    (uint64_t)bus
                );

                serial_print(" device ");

                serial_print_dec(
                    (uint64_t)device
                );

                serial_print(" function ");

                serial_print_dec(
                    (uint64_t)function
                );

                serial_print("\n");

                /*
                 * BAR0 must be MMIO.
                 */
                if (pci_dev.bar0 & 1U) {
                    serial_print(
                        "[e1000] ERROR: BAR0 is I/O space\n"
                    );
                    continue;
                }

                uint64_t bar =
                    (uint64_t)(pci_dev.bar0 & ~0xFULL);

                if (bar == 0) {
                    serial_print(
                        "[e1000] ERROR: invalid BAR0\n"
                    );
                    continue;
                }

                serial_print(
                    "[e1000] BAR0 physical=0x"
                );

                serial_print_hex(bar);
                serial_print("\n");

                /*
                 * The e1000 register block occupies well beyond
                 * the first page. Map a conservative 64 KiB.
                 */
                if (vmm_map_mmio(
                        bar,
                        0x10000) != 0) {

                    serial_print(
                        "[e1000] ERROR: unable to map BAR0\n"
                    );

                    continue;
                }

                struct e1000_device *edev =
                    &e1000_devices[
                        e1000_device_count
                    ];

                memset(
                    edev,
                    0,
                    sizeof(*edev)
                );

                edev->mem_base = bar;
                edev->irq = pci_dev.irq_line;
                edev->bar_type = 0;

                if (e1000_init(edev) != 0) {
                    serial_print(
                        "[e1000] Device initialization failed\n"
                    );
                    continue;
                }

                struct net_device *netdev =
                    net_device_alloc();

                if (!netdev) {
                    serial_print(
                        "[e1000] ERROR: unable to allocate net_device\n"
                    );

                    (void)e1000_close(edev);
                    continue;
                }

                memcpy(
                    netdev->mac,
                    edev->mac,
                    sizeof(netdev->mac)
                );

                netdev->mtu = E1000_MTU;

                netdev->flags =
                    NET_DEV_FLAG_UP |
                    NET_DEV_FLAG_RUNNING |
                    NET_DEV_FLAG_BROADCAST |
                    NET_DEV_FLAG_MULTICAST;

                netdev->priv = edev;

                /*
                 * Correct function-pointer signature.
                 */
                netdev->transmit =
                    e1000_netdev_transmit;

                if (net_device_register(
                        netdev) != 0) {

                    serial_print(
                        "[e1000] ERROR: net_device registration failed\n"
                    );

                    (void)e1000_close(edev);
                    continue;
                }

                /*
                 * Register IRQ only if the PIC/IRQ subsystem
                 * provides a valid line.
                 */
                if (edev->irq < 16) {
                    if (irq_register(
                            edev->irq,
                            e1000_irq_handler,
                            edev) != 0) {

                        serial_print(
                            "[e1000] WARNING: IRQ registration failed\n"
                        );
                    }
                }

                e1000_device_count++;

                serial_print(
                    "[e1000] Driver initialized successfully\n"
                );
            }
        }
    }

    if (e1000_device_count == 0) {
        serial_print(
            "[e1000] No compatible NIC found\n"
        );
    } else {
        serial_print("[e1000] Devices initialized: ");
        serial_print_dec(
            (uint64_t)e1000_device_count
        );
        serial_print("\n");
    }
}