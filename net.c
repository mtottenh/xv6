#include "types.h"
#include "defs.h"
#include "net.h"
#include "e1000.h"

LIST_HEAD(nic_dev);
void net_xmit(unsigned char* devname, void* data, size_t i) {
    struct list_head *pos;
    struct list_head* list = &nic_dev;
    list_for_each(pos, list) {
        struct net_dev* dev =list_entry(pos, struct net_dev, list);
        if (strncmp(dev->name, (const char*) devname,4) == 0) {
            dev->net_xmit(dev, data, i);
        }
    }
}
int
net_get_mac(char* devname, unsigned char* destbuf) {
    struct list_head *pos;
    struct list_head* list = &nic_dev;
    list_for_each(pos, list) {
        struct net_dev* dev =list_entry(pos, struct net_dev, list);
        if (strncmp(dev->name, devname,4) == 0) {
            memmove(destbuf, ((struct e1000_dev*)dev->dev)->mac, 6);
            return 0; 
        }
    }
    return 1;
}
void netintr(int intr) {
	struct list_head* pos;
	struct list_head* list = &nic_dev;
	int handled = 0;
	list_for_each(pos, list) {
		struct net_dev* dev = list_entry(pos, struct net_dev, list);

		if (dev->irq_line == intr) {
			cprintf("[ net ] Calling interrupt handler\n");
			dev->intr_handler(dev, intr);
			handled = 1;
		} else {
            cprintf("Skipping interrupt %d, registered device only handles: %d\n", intr, dev->irq_line);
        }
	}
	if (!handled) {
		cprintf("No device registerd for interrupt %d\n", intr);
	}
}


uint32_t htonl(uint32_t hostlong) {
    uint8_t  data[4] = {0};
    memmove(data, &hostlong, 4);
    return (  ((uint32_t) data[0] << 24)
            | ((uint32_t) data[1] << 16)
            | ((uint32_t) data[2] << 8)
            |  (uint32_t) data[3]);

}
uint16_t htons(uint16_t hostshort) {
    uint8_t  data[2] = {0};
    memmove(data, &hostshort, 2);
    return (  ((uint32_t) data[0] << 8)
            | ((uint32_t) data[1]));
}
uint32_t ntohl(uint32_t netlong) {
    return htonl(netlong);
//    uint8_t data[4] = {0};
//    memmove(data, &netlong, 4);
//    return (data[3]<<0) | (data[2]<<8) | (data[1]<<16) | (data[0]<<24);

}
uint16_t ntohs(uint16_t netshort) {
    return htons(netshort);
}

