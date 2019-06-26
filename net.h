#include "adt/list/double.h"
struct net_dev {
	void (*tx_poll) (struct net_dev* n);
	void (*rx_poll) (struct net_dev* n);
    int (*net_xmit) (struct net_dev* n, void* data, size_t i);
	void (*intr_handler) (struct net_dev* n, int intr_no);
	int irq_line;
	void* dev;
    char name[4];
	struct list_head list;
};

void net_xmit(unsigned char* devname, void* data, size_t i);
int net_get_mac(char* name, unsigned char* destbuf);
void netintr(int intr);


