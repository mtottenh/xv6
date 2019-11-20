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
    volatile uint32_t *a = (uint32_t*) ((mmio_base + addr));
    *a = val;
    asm volatile ("mfence" ::: "memory");
}

uint32_t mmio_read_cmd (uint32_t mmio_base, uint32_t addr) {
    asm volatile ("mfence" ::: "memory");

    return *((volatile uint32_t *)((mmio_base + addr)));
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

    if (rx_desc[d->rx_tail].status != 0) {
        cprintf("[ e1000 ]: rx_poll() - got a packet! \n");
        cprintf("[ e1000 ]: STA %x, CRC %x, LEN: %d, ADDR: %x\n", 
                        rx_desc[d->rx_tail].status,
                        rx_desc[d->rx_tail].checksum,
                        rx_desc[d->rx_tail].length,
                        rx_desc[d->rx_tail].addr_l);
        struct pkt_buf* p = (struct pkt_buf*) p2v(rx_desc[d->rx_tail].addr_l);
        for (int i = 0; i< rx_desc[d->rx_tail].length; i++) {
            cprintf("%x", p->data[i]);
        }
        cprintf("\n");
        rx_desc[d->rx_tail].status = 0;
        d->rx_tail = (d->rx_tail + 1) % NUM_DESCRIPTORS;
        d->write_cmd(d->mmio_base, REG_RDT, d->rx_tail);
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
    while( !( tx_desc[tail].sta & 0xF)) {
        ;
    }
    d->read_cmd(d->mmio_base, REG_ICR);
    return 0;
}

static struct spinlock e1000lock;

int e1000_rx(struct net_dev *n, void* data, size_t len) {
    cprintf("[ e1000 ]: rx()\n");
    struct e1000_dev* d = (struct e1000_dev *)(n->dev);
//    acquire(&e1000lock); 
    uint32_t tail;
///    do {
        tail = d->rx_tail; 
        if (rx_desc[tail].status == 0) {
            cprintf("[ e1000 ]: tail descriptor status is 0. blocking\n");
//            sleep(d,&e1000lock);
        }
//   }  while (rx_desc[tail].status == 0); 
//    release(&e1000lock);
    return 0;
}

void e1000_handle_interrupt(struct net_dev* n, int intr) {
    struct e1000_dev *d = (struct e1000_dev*) (n->dev);

    uint32_t interrupt_cause = d->read_cmd(d->mmio_base, REG_ICR);
    cprintf("[e1000]: Interrupt! %x\n", interrupt_cause);
    if (interrupt_cause & TXQE) {
        cprintf("[e1000]: Interrupt cause - Transmit queue empty\n");
        cprintf("[e1000]: Transmit Q, Head: %d Tail: %d\n",d->read_cmd(d->mmio_base, REG_TDH), d->read_cmd(d->mmio_base, REG_TDT));
    }
    if (interrupt_cause & LSC) {
        cprintf("[e1000]: Interrupt cause - Link status change\n");
        uint32_t ctl = d->read_cmd(d->mmio_base, REG_CTRL);
        ctl |= CTRL_SLU;
        d->write_cmd(d->mmio_base, REG_CTRL, ctl);
    }
    n->rx_poll(n);
    n->tx_poll(n);
}

int e1000_flush_write(struct e1000_dev *d) {
    return d->read_cmd(d->mmio_base, REG_STATUS);
}


static void e1000_rx_set_ra_0_to_mac(struct e1000_dev *d) {
    
    /* 
     * Set Receive Address Register to MAC (48 bits)
     *  - RAL (recieve address low), contains the lower 32 bits of the mac addr.
     *  - RAH (recieve address high), contains upper 16 bits of the mac addr
     *    and flags.
     */
    uint32_t reg_low = ((uint32_t)d->mac[0]) |  ((uint32_t)d->mac[1] << 8) |  
            ((uint32_t)d->mac[2] << 16) | ((uint32_t)d->mac[3] << 24);
    uint32_t reg_high = (uint32_t)d->mac[4] | ((uint32_t)d->mac[5] << 8);
    reg_high |= REG_RAH_AV_MASK;
    E1000_WRITE_REG_ARRAY(d, REG_RAL, 0, reg_low);
    e1000_flush_write(d);
    E1000_WRITE_REG_ARRAY(d, REG_RAH, 0, reg_high);
    e1000_flush_write(d);
}
static void e1000_rx_flush_ra_regs(struct e1000_dev *d) {
    e1000_rx_set_ra_0_to_mac(d);
    for (int i = 1; i < 16; i++) {
        E1000_WRITE_REG_ARRAY(d, REG_RAL, (i << 1), 0x0);
        e1000_flush_write(d);
        E1000_WRITE_REG_ARRAY(d, REG_RAH,  (i << 1), 0x0);
        e1000_flush_write(d);
    }
}
static void e1000_rx_flush_mta_regs(struct e1000_dev* d) {
    /* 
     * Initialise MTA to 0
     * uint32_t Multicast_Table_Array[128] (allows for filtering beyond Recieve
     * address register.)
     */
    uint32_t i;
    for (i = 0; i < 128; i++) {
        E1000_WRITE_REG_ARRAY(d, REG_MTA, i, 0x0);
    }
    e1000_flush_write(d);
}

static void e1000_set_rx_mode(struct e1000_dev *d) {
    uint32_t rctl = d->read_cmd(d->mmio_base, REG_RCTL);
    rctl |= RCTL_UPE;
    rctl |= RCTL_MPE;
    rctl &= ~RCTL_VFE;
    d->write_cmd(d->mmio_base, REG_RCTL, rctl);
    e1000_flush_write(d);
    e1000_rx_flush_ra_regs(d);
    e1000_rx_flush_mta_regs(d);
}
static void e1000_set_manageability(struct e1000_dev *d) {
    /* Disable hardware interception of ARP req/response*/
    uint32_t manc = d->read_cmd(d->mmio_base, MANC);
    manc &= ~MANC_ARP_EN;
    d->write_cmd(d->mmio_base, MANC, manc);
    e1000_flush_write(d); 
}

static void e1000_configure_tx(struct e1000_dev *d) {
    /* Set Base register for tx descriptor ring */
    d->write_cmd(d->mmio_base, REG_TDBAL, V2P(tx_desc));
    d->write_cmd(d->mmio_base, REG_TDBAH, 0x0);
    d->write_cmd(d->mmio_base, REG_TDLEN, NUM_DESCRIPTORS * 16);
    /* Set Head/Tail of tx ring */
	d->write_cmd(d->mmio_base, REG_TDH, 0);
	d->write_cmd(d->mmio_base, REG_TDT, 0);
    /* Set default Inter packet gap values */
    uint32_t tipg =  DEFAULT_82543_TIPG_IPGT_COPPER ;
    tipg |= (DEFAULT_82543_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT);
    tipg |= (DEFAULT_82543_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT);
    d->write_cmd(d->mmio_base, REG_TIPG, tipg);
    uint32_t tctl = d->read_cmd(d->mmio_base, REG_TCTL);
    tctl |=  TCTL_PSP; /* Pad short packets */
    tctl |= TCTL_COLD_DEFAULT | TCTL_CT_DEFAULT;

    d->write_cmd(d->mmio_base, REG_TCTL, tctl);
}

static void e1000_reset_hw(struct e1000_dev *d) {
    uint32_t ctl;
    
    d->write_cmd(d->mmio_base, REG_IMC, 0xffffffff);
    d->write_cmd(d->mmio_base, REG_RCTL, 0);
    d->write_cmd(d->mmio_base, REG_TCTL, TCTL_PSP);
    e1000_flush_write(d);

    ctl = d->read_cmd(d->mmio_base, REG_CTRL);
    ctl |= E1000_CTRL_RST;
    /* Force IO write for reset */
   // d->io_write_cmd(d->io_base, REG_CTRL, ctl  );
    e1000_set_manageability(d);
    d->write_cmd(d->mmio_base, REG_IMC, 0xffffffff);
    d->read_cmd(d->mmio_base, REG_ICR);
}

static void e1000_configure_rctl(struct e1000_dev *d) {
    /*
     *  Set Recieve Crotrol register
     */
    uint32_t rctl = d->read_cmd(d->mmio_base, REG_RCTL);
    rctl &= ~RCTL_BSIZE_4096;
    rctl |= RCTL_BSIZE_2048; /* Default buffer size is 2k*/
    rctl &= ~RCTL_BSEX; /* No size extend */
    rctl |= RCTL_BAM; /* Broadcast Accept Mode */
    rctl |= RCTL_SBP; /* Store Bad Packets */
//    rctl |= RCTL_LBM_NO; /* No loopback mode */
//    rctl |= RCTL_LPE; /* Large Packet Enable */
    rctl &= ~RCTL_VFE; /* No vlan filter enable */
    d->write_cmd(d->mmio_base, REG_RCTL, rctl);
    e1000_flush_write(d);
}
static void e1000_configure_rx(struct e1000_dev *d) {
    uint32_t rctl = d->read_cmd(d->mmio_base, REG_RCTL);
    /* disable recieves while we configure RDH/RDT */
    d->write_cmd(d->mmio_base, REG_RCTL, rctl & ~RCTL_EN);
    for (uint32_t i = 0; i < NUM_DESCRIPTORS; i++) {
        rx_desc[i].addr_l = V2P(& (rx_buf[i])); 
        rx_desc[i].addr_h = 0;
    }
    /* Default values for interrupt delay  registers */
    // d->write_cmd(d->mmio_base, REG_RDTR, 0);
    // d->write_cmd(d->mmio_base, REG_RADV, 8);
    e1000_flush_write(d);
    d->write_cmd(d->mmio_base, REG_RDLEN, NUM_DESCRIPTORS * 16);
    
    d->write_cmd(d->mmio_base, REG_RDBAH, 0x0); 
    d->write_cmd(d->mmio_base, REG_RDBAL, V2P(rx_desc));

    d->write_cmd(d->mmio_base, REG_RDH, 0);
    d->write_cmd(d->mmio_base, REG_RDT, 0);
    d->rx_tail = 0;
    d->write_cmd(d->mmio_base, REG_RCTL, rctl | RCTL_EN);
}
static void e1000_configure(struct e1000_dev *d) {
    
    e1000_set_rx_mode(d);
    
    e1000_set_manageability(d);
    
    e1000_configure_tx(d);
    
    e1000_configure_rctl(d);
    
    e1000_configure_rx(d);

}

static int e1000_detect_mac(struct e1000_dev *e) {
    int has_eeprom = e->detect_eeprom(e);
    if (!has_eeprom)
        return 1;
    cprintf("[e1000]: EEPROM Detected.\n");
    uint16_t tmp = e->eeprom_read(e, 0);
    e->mac[0] = tmp & 0xFF;
    e->mac[1] = tmp >> 8;
    tmp = e->eeprom_read(e, 1);
    e->mac[2] = tmp & 0xFF;
    e->mac[3] = tmp >> 8;
    tmp = e->eeprom_read(e, 2);
    e->mac[4] = tmp & 0xFF;
    e->mac[5] = (tmp >> 8) & 0xFF;
    cprintf("[e1000]: MAC ");
    for (uint32_t i = 0; i < 6; i++) {
        cprintf("%x",e->mac[i]);
        if (i != 5)
            cprintf(":");
    }
    cprintf(" \n");
    return 0;
}
static void e1000_detect_io_type(struct pci_device* p, struct e1000_dev* e) {
    for (int i = 0; i < 1; i++) {
        if (p->base_addr_reg[i] & 0x1) {
            e->io_base = p->base_addr_reg[i] & 0xFFFFFFF0;
            e->io_write_cmd = io_write_cmd;
            e->io_read_cmd = io_read_cmd;
        } else {
            e->mmio_base = p->base_addr_reg[i] & 0xFFFFFFF0;
            e->write_cmd = mmio_write_cmd;
            e->read_cmd = mmio_read_cmd;
            e->eeprom_read = read_eeprom;
            e->detect_eeprom = detect_eeprom; 
        }
    }
}
static void  e1000_enable_irq(struct e1000_dev *d) {

    e1000_flush_write(d);
    d->write_cmd(d->mmio_base, REG_IMS, IMS_ENABLE_MASK);
    e1000_flush_write(d);
    d->read_cmd(d->mmio_base, REG_ICR);
}

static void  e1000_enable_irq_apic(void) {
    picenable(IRQ_NIC);
    ioapicenable(IRQ_NIC, 0);
    ioapicenable(IRQ_NIC, 1);
}
static void e1000_tx_enable(struct e1000_dev *d) {
    uint32_t tctl = d->read_cmd(d->mmio_base, REG_TCTL);
    tctl |= TCTL_EN;
    d->write_cmd(d->mmio_base, REG_TCTL, tctl);
    e1000_flush_write(d);
}
static void e1000_link_sts_change(struct e1000_dev *d) {
    d->write_cmd(d->mmio_base, REG_ICS, ICS_LSC);
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
    e1000_detect_io_type(p,e); 
    e1000_detect_mac(e);
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
     /* Reset the device */ 
 
    cprintf("[e1000]: Resetting device PCI Config.\n");
    e1000_reset_hw(e);
    e1000_flush_write(e);

    e1000_configure(e);
    /*  Set Link up and enable Auto speed detection */
    uint32_t ctl = e->read_cmd(e->mmio_base, REG_CTRL);
    ctl |= CTRL_SLU;
    ctl |= CTRL_ASDE;
    e->write_cmd(e->mmio_base, REG_CTRL, ctl);

    e1000_enable_irq(e);
    e1000_enable_irq_apic();
    e1000_tx_enable(e);
    e->read_cmd(e->mmio_base, REG_ICR);
    e1000_link_sts_change(e);
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


