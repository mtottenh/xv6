#include <adt/list/double.h>
#include <types.h>
#include <defs.h>
#include <param.h>
#include <memlayout.h>
#include <mmu.h>
#include <proc.h>
#include <x86.h>
#include <traps.h>
#include <pci.h>
#include <net.h>
#include <e1000.h>
extern struct list_head pci_drivers;
extern struct list_head nic_dev;
extern int pci_read_word(uint32_t bus, uint32_t dev, uint32_t func, uint8_t reg);
int e1000_rx_init(struct e1000_dev* d);
int e1000_tx_init(struct e1000_dev* d);
#ifndef static_assert
#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)
#endif


#define INTEL_VID 0x8086
#define INTEL_82540EM 0x100e
    // Quick hack From inspection BAR0 is emulated as I/O
    // not MMIO, So for now lets just use that
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
#define REG_MTA 0x5200
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
                    i, (p->base_addr_reg[i] & 0xF));
        
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
            cprintf("%x",e->mac[i]);
            if (i != 5)
                cprintf(":");
        }
        cprintf(" ]\n");
    } else {
        cprintf("[No EEPROM detected]\n");
    }
    picenable(IRQ_NIC);
    ioapicenable(IRQ_NIC, 0);
	/* TODO: Error handling */
    e1000_rx_init(e);
    e1000_tx_init(e);
    e->read_cmd(e->mmio_base, 0xC0); // Clear pending interrupts
	/* TODO: Create net_dev structure & attach */
	struct net_dev* n = (struct net_dev*) kalloc();
	n->tx_poll = e1000_tx_poll;
	n->rx_poll = e1000_rx_poll;
	n->irq_line = p->irq_line;
	n->dev = (void*)e;
	list_add( &n->list, &nic_dev);

    return 0;
}
#define REG_TCTL  0x400
#define REG_TDBAL 0x3800
#define REG_TDBAH 0x3804
#define REG_TDLEN 0x3808
#define REG_TDH   0x3810
#define REG_TDT   0x3818
#define TCTL_EN   (1 << 1)
#define TCTL_PSP  (1 << 3)
int e1000_tx_init(struct e1000_dev* d) {

    int i;
 /* 1. Allocate Transmit Descriptor List */
    static_assert(sizeof(struct legacy_tx_desc) * NUM_DESCRIPTORS <= PGSIZE, "Too many network descriptors");
    d->tx_desc_base = (uint8_t*)kalloc();
    for (i = 0; i < NUM_DESCRIPTORS; i++) {
		d->tx_desc[i] = (struct legacy_tx_desc*) (d->tx_desc_base + i*16);
        d->tx_desc[i]->address = v2p(kalloc());
        d->tx_desc[i]->cmd = 0;
		cprintf("Buffer Address of tx[%d]: %x\n", i, d->tx_desc[i]->address);
    }
	uint32_t tmpaddr = v2p((void*)d->tx_desc_base);
	
    /* 2. Set TDBAL/TDAH to the start of the descriptor list*/
    d->write_cmd(d->mmio_base, REG_TDBAH,(uint32_t) ((uint64_t) tmpaddr >> 32) );
    d->write_cmd(d->mmio_base, REG_TDBAL,(uint32_t) ((uint64_t) tmpaddr & 0xffffffff));
    cprintf("e1000: TDBAH:TDBAL 0x%x:0x%x\n", d->read_cmd(d->mmio_base, REG_TDBAH), d->read_cmd(d->mmio_base, REG_TDBAL)); 
    /* 3. Set TDLEN to the size of the list (/ring buffer) */
    d->write_cmd(d->mmio_base, REG_TDLEN, NUM_DESCRIPTORS * 16);
	d->write_cmd(d->mmio_base, REG_TDH, 0);
	d->write_cmd(d->mmio_base, REG_TDT, NUM_DESCRIPTORS - 1);
	cprintf("e1000: TDH:TDT:TDLEN %d:%d:%d\n", d->read_cmd(d->mmio_base, REG_TDH), d->read_cmd(d->mmio_base, REG_TDT), d->read_cmd(d->mmio_base, REG_TDLEN));
 /* 4. Initialize transmit control register (TCTL) */
    d->write_cmd(d->mmio_base, REG_TCTL, (TCTL_EN | TCTL_PSP) );
 /*   
  * 5. Initialise TIPG register
  */
    return 0;
}
// TODO: Fill in from the manual
#define REG_IMS 0xD0
#define REG_RAL 0x5400
#define REG_RAH 0x5404
#define REG_RDH 0x3810
#define REG_RDT 0x3818
#define REG_RDBAH 0x2804
#define REG_RDBAL 0x2800
#define REG_RDLEN 0x2808
#define REG_CTRL 0x0 
#define REG_RCTL 0x100

#define CTRL_SLU 0x40
/* The control params we want to set:
 */
/* Packet size is a max of 4k because we can't allocate bigger buffers */
#define RCTL_BSIZE_4096     ((3 << 16) | (1 << 25))
/* Enable recieving */
#define RCTL_EN (1 << 1)
/* Broadcast accept mode */
#define RCTL_BAM (1 << 15)
/* Strip the CRC from the ethernet packet */
#define RCTL_SCRC (1 << 26)
/* Enable promisc for unicast/multicast */
#define RCTL_UPE (1 << 3)
#define RCTL_MPE (1 << 4)
/* Enable long packets (16k) */
#define RCTL_LPE (1 << 5)

#define RXO   (1 << 6)  
#define RXT0  (1 << 7)
#define RXDMT (1 << 4)
#define RXSEC (1 << 3)
#define LSC   (1 << 2)
int e1000_rx_init(struct e1000_dev* d) {
    cprintf("Initializing RX Queues\n");
    /* 0. Force link up */
    d->write_cmd(d->mmio_base, REG_CTRL, (d->read_cmd(d->mmio_base, REG_CTRL) | CTRL_SLU));
    
    /* 
     * 1. Set Receive Address Register to MAC
     *  - The upper bits of RAH contain some weird stuff...
     */
    d->write_cmd(d->mmio_base, REG_RAL, *(uint32_t*)d->mac);
    d->write_cmd(d->mmio_base, REG_RAH, *(uint16_t*)&d->mac[4]);

    /* 
     * 2. Initialise MTA to 0
     */
    int i;
    for (i = 0; i < 128; i++) {
        d->write_cmd(d->mmio_base, REG_MTA + i*4, 0);
    }
    /*
     * 3. Program the IMS/R Register to enable interrupts
     */
/*  d->write_cmd(d->mmio_base, REG_IMS, 0x1F6DC);
                        (RXO | RXT0 | RXDMT | RXSEC | LSC)); */ 
    /*
     * 5. Set up Receive descriptor list
     */
    /* Allocate a 4k Page for our descriptor list 
     * We only have 64 16 byte descriptors so one page will do.
     * I think it needs to be contiguous?
     * 16*64 = 1024
     */
    static_assert(sizeof(struct legacy_rx_desc*) * NUM_DESCRIPTORS <= PGSIZE, "Too many network descriptors");
     d->rx_desc_base = (uint8_t*)kalloc();
     /* TODO: Check allocation */
    for (i = 0; i < NUM_DESCRIPTORS; i++) {
		d->rx_desc[i] = (struct legacy_rx_desc*) (d->rx_desc_base + i * 16); 
        d->rx_desc[i]->address = v2p(kalloc());
        d->rx_desc[i]->status = 0;
    }
    uint32_t tmpaddr0 = (uint32_t) v2p((void*)d->rx_desc_base); 
    uint64_t tmpaddr = (uint64_t) tmpaddr0;
    d->write_cmd(d->mmio_base, REG_RDBAH, (uint32_t) (tmpaddr >> 32));
    d->write_cmd(d->mmio_base, REG_RDBAL, (uint32_t) (tmpaddr & 0xffffffff));
    /* 
     * 6. Set RDLEN
     */
     d->write_cmd(d->mmio_base, REG_RDLEN, NUM_DESCRIPTORS * 16);   
     /*
      * 7. Set Recieve descriptor Head/Tail pointers
      */
     d->write_cmd(d->mmio_base, REG_RDH, 0);
     d->write_cmd(d->mmio_base, REG_RDT, NUM_DESCRIPTORS);

     /*
      * 8. Set Recieve Crotrol register
      */
    d->write_cmd(d->mmio_base, REG_RCTL, 
                    ( RCTL_BSIZE_4096 | RCTL_EN | RCTL_BAM | RCTL_SCRC 
                      | RCTL_UPE | RCTL_MPE | RCTL_LPE)); 
    /* 
     * Enable interrupts
     */
    d->write_cmd(d->mmio_base, REG_IMS, 0x1F6DC);
    d->write_cmd(d->mmio_base, REG_IMS, 0xFF & ~4);
    

/*
 * 
 * 3. Program the Interrupt Mask Set/Read (IMS) register to enable any
 *    interrupt the software driver wants to be notified of when the event occurs.
 *    Suggested bits include RXT, RXO, RXDMT, RXSEQ, and LSC. There is no immediate
 *    reason to enable the transmit interrupts.
 * 4. (OPTIONAL) If software uses the Receive Descriptor Minimum Threshold Interrupt, the
 *    Receive Delay Timer (RDTR) register should be initialized with the desired
 *    delay time.
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


