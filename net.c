#include "types.h"
#include "defs.h"
#include "net.h"
LIST_HEAD(nic_dev);

void netintr(int intr) {
	struct list_head* pos;
	struct list_head* list = &nic_dev;
	int handled = 0;
	list_for_each(pos, list) {
		struct net_dev* dev = list_entry(pos, struct net_dev, list);
		if (dev->irq_line == intr) {
			dev->intr_handler(intr);
			handled = 1;
		}
	}
	if (!handled) {
		cprintf("No device registerd for interrupt %d\n", intr);
	}
}
