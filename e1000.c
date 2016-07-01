#include "adt/list/double.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "pci.h"
extern struct list_head pci_drivers;
extern int pci_read_word(uint32_t bus, uint32_t dev, uint32_t func, uint8_t reg);
/*
 * legacy transmit descriptor format
 * Table 3-8 Intel software dev manual.
 * "
 * To select legacy mode operation, bit 29 (TDESC.DEXT) should be set to 0b. In
 * this case, the descriptor format is defined as shown in Table 3-8. The address
 * and length must be supplied by software. Bits in the command byte are optional,
 * as are the Checksum Offset (CSO), and Checksum Start (CSS) fields.
 * "
 */
struct tx_desc
{
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed));

#define INTEL_VID 0x8086
#define INTEL_82540EM 0x100e
    // Quick hack From inspection BAR0 is emulated as I/O
    // not MMIO, So for now lets just use that
    // Apparently BAR0 doesn't work.. Lets use BAR2 instead?
void io_write_cmd(struct pci_device* d, uint32_t addr, uint32_t val) {
    uint32_t io_base = d->base_addr_reg[0] & 0xFFFFFFF0;
    outl(io_base, addr);
    outl(io_base + 4, val);
}

uint32_t io_read_cmd(struct pci_device* d, uint32_t addr) {
    uint32_t io_base = d->base_addr_reg[0] & 0xFFFFFFF0;
    outl(io_base, addr);
    return inl(io_base + 4);
}

void mmio_write_cmd (struct pci_device* d, uint32_t addr, uint32_t val) {
    volatile uint32_t *a = (uint32_t*) (d->io_base + addr);
    *a = val;
}

uint32_t mmio_read_cmd (struct pci_device* d, uint32_t addr) {
    return *((volatile uint32_t *)(d->io_base + addr));
} 

#define REG_EEPROM      0x0014
int detect_eeprom(struct pci_device* d) {
    int has_eeprom = 0;
    int i;
    d->write_cmd(d, REG_EEPROM, 0x1);
    for (i = 0; i < 1000 && !has_eeprom; i++) {
        uint32_t sts = d->read_cmd(d, REG_EEPROM);
        if (sts & 0x10) {
            has_eeprom = 1;
        }
    }
    return has_eeprom;
}
#define RETRIES 1000
int read_eeprom(struct pci_device* d, uint32_t addr) {
    uint32_t tmp = 0, i=0;
    d->write_cmd(d, REG_EEPROM, 0x1 | addr << 8);
    tmp = d->read_cmd(d, REG_EEPROM);
    while ( !((tmp & (1 << 4)) && i < RETRIES)  ) {
        tmp = d->read_cmd(d, REG_EEPROM);
        i++;
    }
    if ( i < RETRIES ) {
        return tmp >> 16;
    } else {
        return 0;
    }
}


int attach_e1000 (struct pci_device *p) {
    int i;
    cprintf("[0x%x:0x%x IRQ Line 0x%x]\n", p->vendor, p->device, 
                   p->irq_line);
    for (i = 0; i < 6; i++) {
        cprintf("[0x%x:0x%x BAR%d - 0x%x]\n", p->vendor, p->device,
                        i, p->base_addr_reg[i] );
        cprintf("[0x%x:0x%x BAR%d Type Bits 0x%x]\n", p->vendor, p->device,
                    i, (p->base_addr_reg[i] & 0x6));
        
    }
    p->io_base = p->base_addr_reg[2];
    p->write_cmd = mmio_write_cmd;
    p->read_cmd = mmio_read_cmd;
    int has_eeprom = detect_eeprom(p);
    if (has_eeprom) {
        cprintf("[Found EEPROM]\n");
        uint16_t mac[6];
        uint16_t tmp = read_eeprom(p, 0);
        mac[0] = tmp & 0xFF;
        mac[1] = tmp >> 8;
        tmp = read_eeprom(p, 1);
        mac[2] = tmp & 0xFF;
        mac[3] = tmp >> 8;
        tmp = read_eeprom(p, 2);
        mac[4] = tmp & 0xFF;
        mac[5] = tmp >> 8;
        cprintf("[0x%x:0x%x MAC ", p->vendor, p->device);
        for (i = 0; i < 6; i++) {
            cprintf("%x",mac[i]);
            if (i != 5)
                cprintf(":");
        }
        cprintf(" ]\n");
    } else {
        cprintf("[No EEPROM detected]\n");
    }
    return 0;
}


int e1000_tx_init() {
/*
 *
 * 1. Allocate Transmit Descriptor List
 * 2. Set TDBAL to the start of the descriptor list
 * 3. Set TDLEN to the size of the list (/ring buffer)
 * 4. Initialize transmit control register (TCTL)
 * 5. Initialise TIPG register
 */
        return 0;
}

int e1000_rx_init() {
/*
 * 1. Receive Address Register (RAL/RAH) must be set to the MAC addr
 *          - This is found somewhere in the EEPROM
 * 2. Initialize MTA to 0b
 * 3. Program the Interrupt Mask Set/Read (IMS) register to enable any
 *    interrupt the software driver wants to be notified of when the event occurs.
 *    Suggested bits include RXT, RXO, RXDMT, RXSEQ, and LSC. There is no immediate
 *    reason to enable the transmit interrupts.
 * 4. (OPTIONAL) If software uses the Receive Descriptor Minimum Threshold Interrupt, the
 *    Receive Delay Timer (RDTR) register should be initialized with the desired
 *    delay time.
 * 5. Set up Recieve Descriptor List 
 * 6. set Recieve descriptor length register to the length of the list
 * 7. Set RDH and RDT.
 * 8. Set RCTL register appropriately.(
 *
 */
    return 0;
}


int e1000_init(void) {
    struct pci_driver *dev;
    dev = (struct pci_driver*)kalloc(); //if struct pci_driver grows beyond a page WE IN TROUBLE :0
    memset(dev, 0, sizeof (struct pci_driver));
    dev->attach = attach_e1000;
    dev->vendor = INTEL_VID;
    dev->device = INTEL_82540EM;
    list_add( &dev->list, &pci_drivers);
/* 1. find mac adress
 * 1. Set Device Control Register (CTRL) values.
 *
 */
    return 0;
}


