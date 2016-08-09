#include "adt/list/double.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "pci.h"
#include "e1000.h"
extern struct list_head pci_drivers;
extern int pci_read_word(uint32_t bus, uint32_t dev, uint32_t func, uint8_t reg);




#define INTEL_VID 0x8086
#define INTEL_82540EM 0x100e
    // Quick hack From inspection BAR0 is emulated as I/O
    // not MMIO, So for now lets just use that
    // Apparently BAR0 doesn't work.. Lets use BAR2 instead?
void io_write_cmd(uint32_t io_base, uint32_t addr, uint32_t val) {
    outl(io_base, addr);
    outl(io_base + 4, val);
}

uint32_t io_read_cmd(uint32_t io_base, uint32_t addr) {
    outl(io_base, addr);
    return inl(io_base + 4);
}

void mmio_write_cmd (uint32_t mmio_base, uint32_t addr, uint32_t val) {
    volatile uint32_t *a = (uint32_t*) (mmio_base + addr);
    *a = val;
}

uint32_t mmio_read_cmd (uint32_t mmio_base, uint32_t addr) {
    return *((volatile uint32_t *)(mmio_base + addr));
} 

#define REG_EEPROM      0x0014
#define REG_MTA	0x5200
uint32_t detect_eeprom(struct e1000_dev* d) {
    int has_eeprom = 0;
    int i;
    d->write_cmd(d->mmio_base, REG_EEPROM, 0x1);
    for (i = 0; i < 1000 && !has_eeprom; i++) {
        uint32_t sts = d->read_cmd(d->mmio_base, REG_EEPROM);
        if (sts & 0x10) {
            has_eeprom = 1;
        }
    }
    return has_eeprom;
}
#define RETRIES 1000
uint16_t read_eeprom(struct e1000_dev* d, uint32_t addr) {
    uint32_t tmp = 0, i=0;
    d->write_cmd(d->mmio_base, REG_EEPROM, 0x1 | addr << 8);
    tmp = d->read_cmd(d->mmio_base, REG_EEPROM);
    while ( !((tmp & (1 << 4)) && i < RETRIES)  ) {
        tmp = d->read_cmd(d->mmio_base, REG_EEPROM);
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
	struct e1000_dev* e = (struct e1000_dev*)kalloc();
	e->dev = p;
    e->mmio_base = p->base_addr_reg[0] & 0xFFFFFFF0;
    if (p->base_addr_reg[0] & 0x1) {
        e->write_cmd = io_write_cmd;
        e->read_cmd = io_read_cmd;
    } else {
        e->write_cmd = mmio_write_cmd;
        e->read_cmd = mmio_read_cmd;
		e->eeprom_read = read_eeprom;
		e->detect_eeprom = detect_eeprom; 
    }
    int has_eeprom = e->detect_eeprom(e);
    if (has_eeprom) {
        cprintf("[Found EEPROM]\n");
        uint8_t mac[6];
        uint16_t tmp = e->eeprom_read(e, 0);
        e->mac[0] = tmp & 0xFF;
        e->mac[1] = tmp >> 8;
        tmp = e->eeprom_read(e, 1);
        e->mac[2] = tmp & 0xFF;
        e->mac[3] = tmp >> 8;
        tmp = e->eeprom_read(e, 2);
        e->mac[4] = tmp & 0xFF;
        e->mac[5] = tmp >> 8;
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
// TODO: Fill in from the manual
#define REG_RAL 0
#define REG_RAH	0
#define REG_RDLEN 0 
int e1000_rx_init(struct e1000_dev* d) {

	/* 
	 * 1. Set Receive Address Register to MAC
	 * 	- The upper bits of RAH contain some weird stuff...
	 */
	d->write_cmd(d->mmio_base, REG_RAL, *(uint32_t*)d->mac);
	d->write_cmd(d->mmio_base, REG_RAH, *(uint16_t*)&d->mac[4]);

	/* 
	 * 2. Initialise MTA to 0
	 */
	int i;
	for (i = 0; i < 128; i++) {
		d->write_cmd(d->mmio_base, REG_MTA, 0);
	}

	/*
	 * 5. Set up Receive descriptor list
	 */
	for (i = 0; i < NUM_DESCRIPTORS; i++) {
		/* 
		 * TODO: Handle < 4k allocations... there will be a tonne of
		 * underutilized space here...
		 * */
		struct legacy_rx_desc* r = (struct legacy_rx_desc*)kalloc();
		d->rx_desc[i] = r;
		r->address = (uint32_t)kalloc(); 
		r->status = 0;
	}

	/* 
	 * 6. Set RDLEN
	 */
   	 d->write_cmd(d->mmio_base, REG_RDLEN, NUM_DESCRIPTORS * 16);	
/*
 * 
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


