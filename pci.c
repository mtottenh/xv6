#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "pci.h"

#define INTEL_VID 0x8086
#define INTEL_82540EM 0x100e
LIST_HEAD(pci_drivers);


int pci_read_word(uint32_t bus, uint32_t dev, uint32_t func, uint8_t reg) {
    uint32_t config_addr = ((bus << 16) ) | ((dev << 11)) | ((func << 8)) | (reg & 0xfc) | ((uint32_t)0x80000000);
    outl(config_addr, PCI_CONFIG_IO);
    return inl(PCI_DATA_IO);
}

void pci_write_word(uint32_t bus, uint32_t dev, uint32_t func, uint32_t reg, uint32_t val) {

    uint32_t config_addr =  ((bus << 16) ) | ((dev << 11)) | ((func << 8)) | (reg & 0xfc) | ((uint32_t)0x80000000);

     outl(config_addr, PCI_CONFIG_IO);
     outl(val, PCI_DATA_IO);
}

static int _find_device(struct list_head* list, int vid, int did, 
                struct pci_device* p) {
    struct list_head* pos;
    struct pci_driver *dev;
    list_for_each( pos, list ) {
        dev = list_entry( pos, struct pci_driver, list );
        if (dev->vendor == vid && dev->device == did) {
            pci_write_word(p->bus, p->dev, p->func, PCI_COMMAND_STATUS_REG,
                    ( PCI_COMMAND_IO_ENABLE 
                      | PCI_COMMAND_MEM_ENABLE 
                      | PCI_COMMAND_MASTER_ENABLE));
            dev->attach(p);
        } else {
        }
    }
    return 0;    
}

int get_pci_info(int bus, int slot, int func) {
    int reg = pci_read_word(bus,slot,func,0);
    uint16_t did = (reg >> 16);
    uint16_t vid = (reg & 0xFFFF);
    int i;
    if (vid != 0xFFFF) {
        cprintf("[PCI Device - 0x%x:0x%x]\n", vid, did);
        struct pci_device *p = (struct pci_device*)kalloc();
        memset(p,0,sizeof (struct pci_device));
        reg = pci_read_word(bus,slot,func, 0x3C);
        p->vendor = vid;
        p->device = did;
        p->irq_line = reg & 0xFF;
        p->bus = bus;
        p->dev = slot;
        p->func = func;
        for (i = 0; i < 6; i++) {
            reg = pci_read_word(bus,slot,func,0x10+i*4);
            p->base_addr_reg[i] = reg;
        }
        _find_device(&pci_drivers, vid, did, p);
    }
    return 0;
}

int pciinit(void) {
    cprintf("Probing for PCI devices\n");
    unsigned int bus, slot, func;
    for ( bus=0; bus<256; bus++ ) {
        for ( slot=0; slot<32; slot++ ) {
            for ( func=0; func<7; func++ ) {
                get_pci_info(bus,slot,func);
            }
        }
    }
    return 0;
}


