#include "adt/list/double.h"
struct net_dev {
	void (*tx_poll) (struct net_dev* n);
	void (*rx_poll) (struct net_dev* n);
	void (*intr_handler) (int intr_no);
	int irq_line;
	void* dev;
	struct list_head list;
};


void netintr(int intr);


