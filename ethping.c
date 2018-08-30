#include "types.h"
#include "stat.h"
#include "user.h"

const char tag[4] = {0xde, 0xad, 0xbe, 0xef};
char packet_buff[1538] = { 0 };

int
main(int argc, char* argv[])
{
	memmove((void*)packet_buff, (void*)tag, 4);
	printf(1, "Calling xmit_packet\n");	
	if(xmit_packet(packet_buff, 1538)) {
		printf(1,"Bad Arg\n");
	}
	exit();
}
