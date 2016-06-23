#include "adt/list/double.h"
#include "types.h"
#include "defs.h"
#include "pci.h"
extern struct list_head pci_drivers;
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

int attach_e1000 (struct pci_device *p) {
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


