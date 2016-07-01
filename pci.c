#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "pci.h"

#define INTEL_VID 0x8086
#define INTEL_82540EM 0x100e
LIST_HEAD(pci_drivers);


static int pci_read_word(uint32_t bus, uint32_t dev, uint32_t func, uint8_t reg) {
    uint32_t config_addr = ((bus << 16) ) | ((dev << 11)) | ((func << 8)) | (reg & 0xfc) | ((uint32_t)0x80000000);
    outl(config_addr, PCI_CONFIG_IO);
    return inl(PCI_DATA_IO);
}

int _find_device(struct list_head* list, int vid, int did) {
    struct list_head* pos;
    struct pci_driver *dev;
    list_for_each( pos, list ) {
        dev = list_entry( pos, struct pci_driver, list );
        if (dev->vendor == vid && dev->device == did) {
            cprintf("[Found driver!]\n");
        } else {
            cprintf("[No Driver found :(]\n");
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
        _find_device(&pci_drivers, vid, did);
        if (vid == INTEL_VID && did == INTEL_82540EM) {
			uint32_t word = pci_read_word(bus,slot,func, 0x3C);

			cprintf("[0x%x:0x%x INT PIN 0x%x INT Line 0x%x]", vid, did, (word & 0xFF), (word & 0xFF00) >> 8 );
            for (i = 0; i < 6; i++) {
                uint32_t bar = pci_read_word(bus,slot,func,10+i*4);
                cprintf("[0x%x:0x%x BAR%d - 0x%x]\n", vid, did, i, bar);
            }
        }
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


