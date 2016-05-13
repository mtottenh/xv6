#define EBDA_BASE_PTR 0x40E
struct acpi_rsdp {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t rev;
    uint32_t  rsdt_addr;
} __attribute__((packed));

struct acpi_sdt_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_rev;
    uint32_t creator_id;
    uint32_t creator_rev;
} __attribute__((packed));

struct acpi_rsdt {
    struct acpi_sdt_header h;
    uint32_t tables;    
} __attribute__((packed));

struct generic_addr_struct {
    uint8_t address_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint32_t addr_upper;
    uint32_t addr_lower;
}__attribute__((packed));

struct acpi_fadt {
    struct acpi_sdt_header h;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t  _resb;
    uint8_t  preffered_pwr_mgmt_profile;
    uint16_t sci_interrupt;
    uint32_t smi_command_port;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4_bios_req;
    uint8_t pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    uint32_t gpe_0_block;
    uint32_t gpe_1_block;
    uint8_t pm1_event_length;
    uint8_t pm1_control_length;
    uint8_t pm2_control_length;
    uint8_t pm_timer_length;
    uint8_t gpe_0_length;
    uint8_t gpe_1_length;
    uint8_t gpe_1_base;
    uint8_t cstate_control;
    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alarm;
    uint8_t month_alarm;
    uint8_t century;
    uint16_t boot_arch_flags; //unused in ACPI1.0
    uint8_t _resb2;
    uint32_t flags;
    struct generic_addr_struct reset_reg;
    uint8_t reset_value;
    uint8_t _resb3[3];
} __attribute__((packed));

struct acpi_dsdt {
    struct acpi_sdt_header h;
    char definition_block[];
}__attribute__((packed));


struct _pci_addr {
    uint64_t base_addr;
    uint16_t pci_seg_num;
    uint8_t pci_start_bus;
    uint8_t pci_end_bus;
    uint32_t resb;
}__attribute__((packed));

struct acpi_mcfg {
    struct acpi_sdt_header h;
    uint8_t resb[8];
    struct _pci_addr p;
} __attribute__((packed));


