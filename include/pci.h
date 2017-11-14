#include <adt/list/double.h>

#define PCI_CONFIG_IO 0xCF8
#define PCI_DATA_IO 0xCFC
struct pci_device {
    
    uint16_t vendor;
    uint16_t device;
    
    uint16_t subsystem_id;
    uint16_t subsystem_vendor_id;

    uint32_t base_addr_reg[6];
    uint32_t base_addr_size[6];
    uint8_t irq_line;
};

struct pci_driver {
    uint16_t vendor;
    uint16_t device;
    int (*attach) (struct pci_device* p);
    struct list_head list;
};