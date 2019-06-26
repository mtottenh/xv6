#include "types.h"
#include "stat.h"
#include "user.h"
#include "ether.h"
#include "arp.h"

#define MAX_ETHER_SIZE 1538
#define MIN_ETHER_SIZE 48
char ETHER_BCAST_ADDR[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; 
char packet_buff[MIN_ETHER_SIZE] = { 0 };

int
main(int argc, char* argv[])
{
    struct eth_hdr* ehdr = (struct eth_hdr*) packet_buff;
    unsigned char mac[6] = { 0 };
    get_mac("eth0", mac);
    printf(1, "Test: %x\n", 0x9a);
    printf(1, "Mac address of eth0: ");
    for (int i = 0; i < 6; i++) {
        printf(1,"%x", mac[i]);
        if (i+1 != 6) 
            printf(1,":");
    }
    printf(1, "\n");
    memmove(ehdr->smac, mac, 6);

    memmove(ehdr->dmac, ETHER_BCAST_ADDR, 6);
    ehdr->ethertype = htons(ETH_P_ARP);
    struct arp_hdr *a= (struct arp_hdr*) ehdr->payload;
    a->hwtype = htons(ARP_ETHER);
    a->proto = htons(ARP_IPv4);
    a->hwlen = 6;
    a->protolen = 4;
    a->opcode = htons(1); // Arp request
    struct arp_ipv4_hdr *i = (struct arp_ipv4_hdr*)a->data;
    memmove(i->shwaddr, mac, 6);
    memmove(i->dhwaddr, ETHER_BCAST_ADDR, 6);
    i->sipaddr = inet_aton("192.168.122.3");
    i->dipaddr = inet_aton("255.255.255.255");
	printf(1, "Calling xmit_packet\n");	
 for (int i = 0; i < 128; i++) {
	if(xmit_packet("eth0", packet_buff, MIN_ETHER_SIZE)) {
		printf(1,"Bad Arg\n");
	}
 }
	exit();
}
