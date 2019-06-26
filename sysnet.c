//
// networking system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"
#include "net.h"
// sys_xmit_packet(buff,size);
int
sys_xmit_packet(void)
{
	unsigned char* buf, *name;
	int i,j;
	if (argint(2, &i) < 0) {
		return -1;
	}
	if (argptr(1, (void*)&buf, sizeof(char*)) < 0) {
		return -1;
	}
    if (argptr(0, (void*)&name, sizeof(char*)) < 0) {
        return -1;
    }
  //  cprintf("Contents of packet:");
	for (j = 0; j < i; j++) {
		cprintf("0x%x ",buf[j]);
	}
    cprintf("\n");
    net_xmit(name, buf, i);
	return 0;
};

int sys_get_mac(void)
{
        char *name;
        unsigned char* destbuf;
        if (argptr(0, (void*)&name, sizeof(char*)) <0) {
            return -1;
        }
        if (argptr(1, (void*)&destbuf, sizeof(char*)) <0) {
            return -1;
        }
        return net_get_mac(name, destbuf);
}
