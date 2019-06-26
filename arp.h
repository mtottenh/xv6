#include "types.h"
#define ARP_ETHER 0x0001
#define ARP_IPv4 0x0800
struct arp_hdr {
    uint16_t hwtype;  // will be ethernet
    uint16_t proto;   // IPv4
    uint8_t hwlen;      //size of macaddr in bytes (6)
    uint8_t protolen;   // size of ipaddr in bytes (4)
    uint16_t opcode;    //Type of ARP message
    unsigned char data[];

} __attribute__((packed));

struct arp_ipv4_hdr {
    unsigned char shwaddr[6]; 
    uint32_t sipaddr;
    unsigned char dhwaddr[6];
    uint32_t dipaddr;
} __attribute__((packed));
