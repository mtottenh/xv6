//
// networking system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include <types.h>
#include <defs.h>
#include <param.h>
#include <stat.h>
#include <mmu.h>
#include <proc.h>
#include <fs.h>
#include <file.h>
#include <fcntl.h>

// sys_xmit_packet(buff,size);
int
sys_xmit_packet(void)
{
	char* buf;
	int i,j;
	if (argint(1, &i) < 0) {
		return -1;
	}
	if (argptr(0, (void*)&buf, i) < 0) {
		return -1;
	}
    cprintf("Contents of packet:");
	for (j = 0; j < i; j++) {
		cprintf("%x",buf[j]);
	}	
	return 0;
};
