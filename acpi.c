#include "types.h"
#include "defs.h"
#include "acpi.h"
#include "memlayout.h"



static
struct acpi_sdt_header* _dump_sdt_header(uchar* p, int sig_len)
{
    int i;
    struct acpi_sdt_header* r = (struct acpi_sdt_header*)p;
    cprintf("[ Address: 0x%x", p);
    if ((uint)p < KERNBASE || (uint)p + sizeof(struct acpi_sdt_header) > KERNBASE + PHYSTOP) {
        cprintf(" - out of range ]\n");
        return 0;
    }
    cprintf("\tSignature: ");
    for (i = 0; i < sig_len; i++) {
        cprintf("%c", p[i]);
    }
    cprintf("\tChecksum Val: %x]\n", r->checksum);
    cprintf("[ OEM ID: ");
    for (i = 0; i < 6; i++) {
            cprintf("%c", r->oem_id[i]);
    }
    cprintf("\trevision: %x\tLength: %d\tNum Tables: %d ]\n", r->revision, r->length,
                    (r->length - sizeof(struct acpi_sdt_header)) / 4);
    return r;
}

static
struct acpi_rsdp* _dump_rsdp_header(uchar* p, int sig_len)
{
    int i;
    struct acpi_rsdp* r = (struct acpi_rsdp*)p;
    cprintf("[ Address: 0x%x", p);
    if ((uint)p < KERNBASE || (uint)p > KERNBASE + PHYSTOP) {
        cprintf(" - out of range ]\n");
        return 0;
    }
    cprintf("\tSignature: ");
    for (i = 0; i < sig_len; i++) {
        cprintf("%c", p[i]);
    }
    cprintf("\tChecksum Val: %x]\n", r->checksum);
    cprintf("[ OEM ID: ");
    for (i = 0; i < 6; i++) {
            cprintf("%c", r->oem_id[i]);
    }
    cprintf("\trevision: %x", r->rev);
    cprintf("\tRSDT Address: %x ]\n\n", p2v(r->rsdt_addr)); 
    return r;
}


static int
_verify_checksum(uchar* p, uint len) {
    uint checksum, n;
    cprintf("Verifying %x bytes from 0x%x - ",len,p);
    
    for (checksum = 0, n=0; n < len; n++)
        checksum += p[n];
    if (!( (checksum & 0xff) == 0) || len == 0) {
        cprintf("Checksum failed\n");
        return 0;
    }
    cprintf("Verified\n");
    return 1;
}
void* _find_rsdp(uint base, uint len) {
    uchar *p;
    for (p = p2v(base); len >= sizeof(struct acpi_rsdp); len -= 4, p += 16) {
        if (!memcmp(p, "RSD PTR ", 8)) {
            // Check the checksum.
            cprintf("\t - Found RSDP at 0x%x\n\n", p);
            if (!_dump_rsdp_header(p,8))
                return 0;
            if (_verify_checksum(p,20)) {
                return p;
            }
        }
    }
    return 0;
}

static
struct acpi_rsdp* _scan_ebda(void) 
{
    uint ebda_ptr = (*((ushort*)p2v(EBDA_BASE_PTR))) << 4;
    struct acpi_rsdp* table = _find_rsdp(ebda_ptr, 1024);
    if (table)
        cprintf("Found RSD PTR at: 0x%x", table);
    return table;
}



static struct acpi_rsdp* _scan_bios_low_mem() {
    return _find_rsdp(0xE0000, 0x1FFFF);
}


inline static
struct acpi_rsdp* _scan_for_rsdp(void)
{
    struct acpi_rsdp* r = _scan_ebda();
    return r ? r : _scan_bios_low_mem();
}


static int
_dump_fadt(struct acpi_fadt* f) {
    return 0;
}

static int
_dump_dsdt(struct acpi_dsdt* f) {

    return 0;
}

/*
static int
_dump_mcfg(struct acpi_mcfg* m) {
    uint32_t num_pci_spaces = (m->h.length - sizeof(struct acpi_sdt_header) - 8) / sizeof(struct _pci_addr);
    struct _pci_addr* p = &(m->p);
    int i;
    for (i = 0; i < num_pci_spaces; i++)
        cprintf("[MCFG: BASE_ADDR 0x%x\tSTART BUS: %d\tEND BUS: %d]\n", p[i].base_addr,
                        p[i].pci_start_bus, p[i].pci_end_bus);
    return 0;
}*/
        
static int
_recursive_table_scan(struct acpi_rsdt* r)
{
    int i, table_entries;
    char sig[5] = {0};
    if ((uint)r < KERNBASE || (uint)r > KERNBASE + PHYSTOP) {
        cprintf(" 0x%x - out of range \n", r);
        return 0;
    }
    if (_verify_checksum((uchar*)r,r->h.length)) {
        _dump_sdt_header((uchar*)r,4);
        memmove(sig,r->h.signature,4);
        table_entries = (r->h.length - sizeof(struct acpi_sdt_header)) / 4;
        uint32_t *t = &(r->tables);
        if (memcmp(sig,"FACP",4) == 0) {
            _dump_fadt((struct acpi_fadt*)r);
            _recursive_table_scan((struct acpi_rsdt*) p2v(((struct acpi_fadt*)r)->dsdt));
        } else if (memcmp(sig, "DSDT", 4) == 0) {
            _dump_dsdt((struct acpi_dsdt*)r);
        } else if (memcmp(sig, "SSDT", 4) == 0) {
            _dump_dsdt((struct acpi_dsdt*)r); //TODO: define ssdt struct
        } else if (memcmp(sig, "APIC", 4) == 0) {
            _dump_dsdt((struct acpi_dsdt*)r); //TODO: define apic struct
        } else if (memcmp(sig, "HPET", 4) == 0) {
            _dump_dsdt((struct acpi_dsdt*)r); //TODO: define hpet struct
        } else if (memcmp(sig, "MCFG", 4) == 0) {
//            _dump_mcfg((struct acpi_mcfg*)r);
              _dump_dsdt((struct acpi_dsdt*)r);
        } else {
            for (i = 0; i < table_entries; i++) {
                cprintf( "\n[ %s Next Table at: 0x%x]\n", sig, p2v(t[i]));
                _recursive_table_scan((struct acpi_rsdt*) p2v(t[i]));
            }
        }
    }
    return 0;    
}

int acpiinit(void)
{
    cprintf("acpiinit(): Called\n");
    struct acpi_rsdp* r = _scan_for_rsdp();
    if (!r)
        return 0;
    cprintf("\n[ Scanning ACPI tables starting from address: 0x%x ]\n\n", p2v(r->rsdt_addr));
    _recursive_table_scan((struct acpi_rsdt*)p2v(r->rsdt_addr));
    return 1;
}
