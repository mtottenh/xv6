#include "types.h"
// Device registers
#define REG_EEPROM      0x0014
#define REG_MTA(i) ((uint32_t*) (d->mmio_base + 0x5200))[i]
#define REG_TCTL  0x400
#define REG_TDBAL 0x3800
#define REG_TDBAH 0x3804
#define REG_TDLEN 0x3808
#define REG_TDH   0x3810
#define REG_TDT   0x3818
#define REG_TIPG  0x0410
#define REG_ICR 0x00c0
#define E1000_CTRL_RST 0x04000000 
#define TCTL_EN   (1 << 1)
#define TCTL_PSP  (1 << 3)

#define TCTL_COLD_DEFAULT (0x200 << 12)
#define TCTL_CT_DEFAULT (0x0F << 4)
// Interrupt mask
#define REG_IMS 0xD0
// Recieve address low/high
#define REG_RAL 0x5400
#define REG_RAH 0x5404
// Address valid mask (bit 32)
#define REG_RAH_AV_MASK 0x80000000 
#define REG_RDH 0x3810
#define REG_RDT 0x3818
#define REG_RDBAH 0x2804
#define REG_RDBAL 0x2800
#define REG_RDLEN 0x2808
#define REG_CTRL 0x0 
#define REG_RCTL 0x100

// Auto Speed Detection Enable (bit 5)
#define CTRL_ASDE 0x20
// Set Link Up (Bit 6)
#define CTRL_SLU 0x40
/* Packet size is a max of 4k because we can't allocate bigger buffers */
#define RCTL_BSIZE_4096     ((0x11 << 16) | (1 << 25))
#define RCTL_BSIZE_2048      (0x0 << 16)// default
/* Enable recieving */
#define RCTL_EN (1 << 1)
/* Broadcast accept mode */
#define RCTL_BAM (1 << 15)
/* Strip the CRC from the ethernet packet */
#define RCTL_SECRC (1 << 26)
/* Enable promisc for unicast/multicast */
#define RCTL_UPE (1 << 3)
#define RCTL_MPE (1 << 4)
/* Enable long packets (16k) */
#define RCTL_LPE (1 << 5)

// Interupt mask register bits.
#define RXO   (1 << 6)  // Receiver FIFO Overrun.
#define RXT0  (1 << 7)  // Receiver Timer Interrupt.
#define RXDMT (1 << 4)  // Receive Descriptor Minimum Threshold hit
#define RXSEQ (1 << 3)  // Receive sequence error.
#define LSC   (1 << 2)  // Link status change
#define TXQE  (1 << 1)  // Transmit Queue Empty
/* This is probably very low but it's a complete guess
 */
#define NUM_DESCRIPTORS 128
/*
 * The set of registers that manage the tx_queue
 */
struct tx_ring_regs {
	uint32_t td_bal;  // Base addr lower 32 bits
	uint32_t td_bah; // base addr higher 32 bits
	uint32_t td_len;  // total length of the descriptor ring
	uint32_t td_head; // Offset from base ( is always a multiple of sizeof(tx_desc))
	uint32_t td_tail; // offset from base (as above)

};

/*
 * legacy transmit descriptor format
 * the element of the tx_queue
 * Table 3-8 Intel software dev manual.
 * "
 * To select legacy mode operation, bit 29 (TDESC.DEXT) should be set to 0b. In
 * this case, the descriptor format is defined as shown in Table 3-8. The address
 * and length must be supplied by software. Bits in the command byte are optional,
 * as are the Checksum Offset (CSO), and Checksum Start (CSS) fields.
 * "
 * TOTAL_SIZE: 16 Bytes
 */
struct legacy_tx_desc 
{
    uint32_t addr_l;  //Address of the Buffer/Frame to transmit in the host memory
    uint32_t addr_h;
    uint16_t length; // One ethernet frame ~1500 bytes
    uint8_t cso;  // Checksum Offset
    uint8_t cmd; //cmd/status field
	uint8_t sta;  //upper 4 bits are reserved bits field
    uint8_t css; //Checksum Start Field
    uint16_t special; //Special Field
} __attribute__((packed));

/*
 * "The TCP/IP context transmit descriptor provides access to the enhanced
 * checksum offload facility available in the Ethernet controller. This feature
 * allows TCP and UDP packet types to be handled more efficiently by performing
 * additional work in hardware, thus reducing the software overhead associated
 * with preparing these packets for transmission."
 *
struct tcp_ip_ctxt_tx_desc_0 {
	uint8_t ipcss;
	uint8_t ipcso;
	uint16_t ipcse;
	uint8_t tucss;
	uint8_t tucso;
	uint16_t tucse;
	uint32_t tucmd_dtyp_paylen;
	uint8_t sta_rsv;
	uint8_t hdrlen;
	uint16_t mss;
} __attribute__((packed));
*/
/*
 * TOTAL SIZE: 16 - BYTES
 */
struct legacy_rx_desc {
	uint32_t addr_h;
    uint32_t addr_l;
	uint16_t length;
	uint16_t checksum;
	uint8_t status;
	uint8_t errors;
	uint16_t special;
}__attribute__((packed));

struct pkt_buf {
    char data[2046];
};

struct e1000_dev {
    /* TODO: The following two fields can really be unified*/



	uint32_t mmio_base;
	uint32_t io_base;
	struct pci_device* dev;
	uint8_t mac[6];
    void (*write_cmd) (uint32_t, uint32_t, uint32_t);
    uint32_t (*read_cmd) (uint32_t, uint32_t);
	uint16_t (*eeprom_read) (struct e1000_dev*, uint32_t);
	uint32_t (*detect_eeprom) (struct e1000_dev*);
	volatile uint8_t* tx_desc_base;
	volatile uint8_t* rx_desc_base;



};

