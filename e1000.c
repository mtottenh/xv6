#include "adt/list/double.h"
#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "pci.h"
#include "net.h"
#include "e1000.h"
extern struct list_head pci_drivers;
extern struct list_head nic_dev;
extern int pci_read_word(uint32_t bus, uint32_t dev, uint32_t func, uint8_t reg);
int e1000_rx_init(struct e1000_dev* d);
int e1000_tx_init(struct e1000_dev* d);
int e1000_tx(struct net_dev *d, void* data, size_t len); 
int e1000_rx(struct net_dev *d, void* data, size_t len); 
volatile struct legacy_tx_desc tx_desc[NUM_DESCRIPTORS] __attribute__((aligned(16))) = { 0 };
volatile struct legacy_rx_desc rx_desc[NUM_DESCRIPTORS]  __attribute__((aligned(16))) = { 0 };
struct pkt_buf rx_buf[NUM_DESCRIPTORS]  __attribute__((aligned(16))) = { 0 };
struct pkt_buf tx_buf[NUM_DESCRIPTORS]  __attribute__((aligned(16))) = { 0 };
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
    asm volatile ("mfence" ::: "memory");
}

uint32_t mmio_read_cmd (uint32_t mmio_base, uint32_t addr) {
    asm volatile ("mfence" ::: "memory");
    return *((volatile uint32_t *)(mmio_base + addr));
} 


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

void e1000_rx_poll(struct net_dev* n) {
    cprintf("[ e1000 ]: rx_poll()\n");
    struct e1000_dev* d = (struct e1000_dev *)(n->dev); 
    //uint32_t head =  d->read_cmd(d->mmio_base, REG_RDH);
    uint32_t tail = d->read_cmd(d->mmio_base, REG_RDT);
    if (rx_desc[tail].status != 0) {
        cprintf("[ e1000 ]: rx_poll() - got a packet! \n");
        cprintf("[ e1000 ]: STA %x, CRC %x, LEN: %d, ADDR: %x\n", 
                        rx_desc[tail].status,
                        rx_desc[tail].checksum,
                        rx_desc[tail].length,
                        rx_desc[tail].addr_l);
        struct pkt_buf* p = (struct pkt_buf*) p2v(rx_desc[tail].addr_l);
        for (int i = 0; i< rx_desc[tail].length; i++) {
            cprintf("%x", p->data[i]);
        }
        cprintf("\n");
    }

}
void e1000_tx_poll(struct net_dev* d) {
    cprintf("[ e1000 ]: tx_poll()\n");
    
}
# define TX_MAX_PKT_SIZE 2048
# define TX_CMD_RS  (1 << 3)
# define TX_CMD_IFCS (1 << 1)
# define TX_CMD_EOP (1 << 0)
# define TX_STA_DD  (1 << 0)

int e1000_tx(struct net_dev *n, void* data, size_t len) {
    struct e1000_dev* d = (struct e1000_dev *)(n->dev); 
    uint32_t tail = d->read_cmd(d->mmio_base, REG_TDT);
    cprintf("[e1000]: Current tail pointer: %d\n", tail);
    if (len > TX_MAX_PKT_SIZE) {
            return -1;
    }
    if ((tx_desc[tail].cmd & TX_CMD_RS) && 
        ! (tx_desc[tail].sta & TX_STA_DD)) {
        cprintf("[e1000]: ERROR: Transmit queue full\n"); 
        return -1; 
    }
    memmove(&(tx_buf[tail]), data, len);
//    memset((void*)&(tx_desc[tail]), 0, sizeof(struct legacy_tx_desc));

    tx_desc[tail].length = len;
    tx_desc[tail].addr_l = V2P(&(tx_buf[tail]));
    tx_desc[tail].addr_h = 0;
    tx_desc[tail].cmd |= TX_CMD_RS | TX_CMD_EOP | TX_CMD_IFCS;
    tx_desc[tail].cso = 0;
    tx_desc[tail].sta &= ~TX_STA_DD;
    cprintf("[e1000]: e1000_tx() - Descriptor %d, Data addr = %p\n", tail, tx_desc[tail].addr_l); 
    cprintf("[e1000]: Updating tail pointer to %d\n", (tail + 1) % NUM_DESCRIPTORS);
    d->write_cmd(d->mmio_base, REG_TDT, (tail + 1) % NUM_DESCRIPTORS);

    return 0;
}
static struct spinlock e1000lock;

int e1000_rx(struct net_dev *n, void* data, size_t len) {
    cprintf("[ e1000 ]: rx()\n");
    struct e1000_dev* d = (struct e1000_dev *)(n->dev);
    acquire(&e1000lock); 
    uint32_t tail;
    do {
        tail = d->read_cmd(d->mmio_base, REG_RDT);
        if (rx_desc[tail].status == 0) {
            cprintf("[ e1000 ]: tail descriptor status is 0. blocking\n");
            sleep(d,&e1000lock);
        }
    }  while (rx_desc[tail].status == 0); 
    release(&e1000lock);
    return 0;
}

void e1000_handle_interrupt(struct net_dev* n, int intr) {
    struct e1000_dev *d = (struct e1000_dev*) (n->dev);
    uint32_t interrupt_cause = d->read_cmd(d->mmio_base, REG_ICR);
    if (interrupt_cause & TXQE) {
        cprintf("[e1000]: Interrupt cause - Transmit queue empty\n");
        cprintf("[e1000]: Transmit Q, Head: %d Tail: %d\n",d->read_cmd(d->mmio_base, REG_TDH), d->read_cmd(d->mmio_base, REG_TDT));
    }
    if (interrupt_cause & LSC) {
        cprintf("[e1000]: Interrupt cause - Link status change\n");
    }
    n->rx_poll(n);
    n->tx_poll(n);
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
#define PCI_COMMAND_STATUS_REG          0x04
#define PCI_COMMAND_IO_ENABLE           0x00000001
#define PCI_COMMAND_MEM_ENABLE          0x00000002
#define PCI_COMMAND_MASTER_ENABLE   0x00000004
    pci_write_word(p->bus, p->dev, p->func, PCI_COMMAND_STATUS_REG,
                    ( PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE));
    struct e1000_dev* e = (struct e1000_dev*)kalloc();
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
        e->mac[5] = (tmp >> 8) & 0xFF;
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
    /* Reset the device */
    cprintf("[e1000]: Resetting device PCI Config\n");
    e->write_cmd(e->mmio_base, REG_CTRL, (e->read_cmd(e->mmio_base, REG_CTRL) | E1000_CTRL_RST));
    while ( e->read_cmd(e->mmio_base, REG_CTRL) & E1000_CTRL_RST ) {
        cprintf(".");
    }
    cprintf("\n[e1000]: Done.\n");
    /* 0. Set Link up and enable Auto speed detection */
    e->write_cmd(e->mmio_base, REG_CTRL, 
                    (e->read_cmd(e->mmio_base, REG_CTRL) | CTRL_SLU | CTRL_ASDE));

	/* TODO: Error handling */
    e1000_tx_init(e);
    e1000_rx_init(e);

    e->read_cmd(e->mmio_base, 0xC0); // Clear pending interrupts
	/* TODO: Create net_dev structure & attach */
	struct net_dev* n = (struct net_dev*) kalloc();
	n->tx_poll = e1000_tx_poll;
	n->rx_poll = e1000_rx_poll;
    n->net_xmit = e1000_tx;
    n->net_recv = e1000_rx;
	n->intr_handler = e1000_handle_interrupt;

	n->irq_line = p->irq_line;
	n->dev = (void*)e;
    n->name[0] = 'e' ;
    n->name[1] = 't' ;
    n->name[2] = 'h' ;
    n->name[3] = '0' ;
	list_add( &n->list, &nic_dev);
    picenable(IRQ_NIC);
    ioapicenable(IRQ_NIC, 0);
    ioapicenable(IRQ_NIC, 1);
    return 0;
}
int e1000_tx_init(struct e1000_dev* d) {

 /* 1. Allocate Transmit Descriptor List */
    static_assert(sizeof(struct legacy_tx_desc) * NUM_DESCRIPTORS <= PGSIZE, "Too many network descriptors");

    cprintf("[ e1000 ]: Tx descriptor ring base address: %p\n", V2P(tx_desc));
    d->write_cmd(d->mmio_base, REG_TDBAL, V2P(tx_desc));
    d->write_cmd(d->mmio_base, REG_TDBAH, 0x0);
    /* 3. Set TDLEN to the size of the list (/ring buffer) */
    d->write_cmd(d->mmio_base, REG_TDLEN, NUM_DESCRIPTORS * 16);
	d->write_cmd(d->mmio_base, REG_TDH, 0);
	d->write_cmd(d->mmio_base, REG_TDT, 0);

 /* 4. Initialize transmit control register (TCTL) */

    d->write_cmd(d->mmio_base, REG_TCTL, (TCTL_EN | TCTL_PSP | TCTL_COLD_DEFAULT | TCTL_CT_DEFAULT) );
    cprintf("[ e1000 ]: TCTL: %x\n", d->read_cmd(d->mmio_base, REG_TCTL));
 /*   
  * 5. Initialise TIPG register (inter packet gap, manual says 10 for IPGT )
  */
//    d->write_cmd(d->mmio_base, REG_TIPG, 10); 
    return 0;
}
// TODO: Fill in from the manual


int e1000_rx_init(struct e1000_dev* d) {
    cprintf("Initializing RX Queues\n");

    
    /* 
     * 1. Set Receive Address Register to MAC (48 bits)
     *  - RAL (recieve address low), contains the lower 32 bits of the mac addr.
     *  - RAH (recieve address high), contains upper 16 bits of the mac addr and some flags.
     */

    uint32_t *reg_low = (uint32_t *) &(d->mac[0]);
    uint16_t *reg_high = (uint16_t *) &(d->mac[4]);
    d->write_cmd(d->mmio_base, REG_RAL, *reg_low);
    d->write_cmd(d->mmio_base, REG_RAH, REG_RAH_AV_MASK || (uint32_t ) (*reg_high));

    /* 
     * 2. Initialise MTA to 0
     * uint32_t Multicast_Table_Array[128] (allows for filtering beyond Recieve address register.
     *  - Initialize to 0
     */

    uint32_t i;
    for (i = 0; i < 128; i++) {
        REG_MTA(i) = 0;
    }

    /*
     * 5. Set up Receive descriptor list
     */
    /* Allocate a 4k Page for our descriptor ring
     * We only have 64 16 byte descriptors so one page will do.
     */
    static_assert(sizeof(struct legacy_rx_desc*) * NUM_DESCRIPTORS <= PGSIZE, "Too many network descriptors");
     /* TODO: Check allocation */
    
    if ((v2p((void*)rx_desc) & 0xf) != 0) {
        cprintf("[e1000]: ERROR: Recieve descriptor ring not 16bit aligned");
    }
    if ((v2p((void*)rx_buf) & 0xf) != 0) {
        cprintf("[e1000]: ERROR: Recieve buffer ring not 16bit aligned");
    }
    for (i = 0; i < NUM_DESCRIPTORS; i++) {
        rx_desc[i].addr_l = V2P(& (rx_buf[i])); 
        rx_desc[i].addr_h = 0;
    }

    d->write_cmd(d->mmio_base, REG_RDBAL, V2P(rx_desc));
    d->write_cmd(d->mmio_base, REG_RDBAH, 0x0); 
    /* 
     * 6. Set RDLEN - Number of bytes allocated to the recieve discriptor buffer.
     *  NUM_DESCRIPTORS * 16 
     */
     d->write_cmd(d->mmio_base, REG_RDLEN, NUM_DESCRIPTORS * 16);   
     /*
      * 7. Set Recieve descriptor Head/Tail pointers
      */
     d->write_cmd(d->mmio_base, REG_RDH, 0);
     d->write_cmd(d->mmio_base, REG_RDT, NUM_DESCRIPTORS - 1);

     /*
      * 8. Set Recieve Crotrol register
      * - Set packet size to 2048 (BSIZE_2048)
      * - Enable packet reception (EN)
      * - Enable long packet reception for packets > 1522 bytes (LPE)
      * - 
      */
    d->write_cmd(d->mmio_base, REG_RCTL, 
                    ( RCTL_BSIZE_2048 | RCTL_EN | RCTL_SECRC | RCTL_LPE)); 
    /* 
 * 3. Program the Interrupt Mask Set/Read (IMS) register to enable any
 *    interrupt the software driver wants to be notified of when the event occurs.
 *    Suggested bits include RXT, RXO, RXDMT, RXSEQ, and LSC. There is no immediate
 *    reason to enable the transmit interrupts.
     */
    d->write_cmd(d->mmio_base, REG_IMS, (RXO | RXT0 | RXDMT | RXSEQ | TXQE));

/*
 * 

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
    initlock(&e1000lock, "e1000");
    memset(dev, 0, sizeof (struct pci_driver));
    dev->attach = attach_e1000;
    dev->vendor = INTEL_VID;
    dev->device = INTEL_82540EM;
    list_add( &dev->list, &pci_drivers);

    return 0;
}


