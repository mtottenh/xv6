#include "types.h"

/* This is probably very low but it's a complete guess
 */
#define NUM_DESCRIPTORS 64
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
    uint64_t address;  //Address of the Buffer/Frame to transmit in the host memory
    uint16_t length; // One ethernet frame ~1500 bytes
    uint8_t cso;  // Checksum Offset
    uint8_t cmd;
	uint8_t status_reserved;  //Command field
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
	uint64_t address;
	uint16_t length;
	uint16_t checksum;
	uint8_t status;
	uint8_t errors;
	uint16_t special;
}__attribute__((packed));

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
	volatile struct legacy_tx_desc* tx_desc[NUM_DESCRIPTORS];
	volatile struct legacy_rx_desc* rx_desc[NUM_DESCRIPTORS];
};

