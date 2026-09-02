#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include "cpu.h"
#include <86box/mem.h>
#include <86box/smram.h>
#include <86box/io.h>
#include <86box/device.h>
#include <86box/pci.h>
#include <86box/chipset.h>
#include <86box/plat_unused.h>
#include <86box/spd.h>
#include <86box/agpgart.h>

#ifdef ENABLE_VIA_VT8371_LOG
int via_vt8371_do_log = ENABLE_VIA_VT8371_LOG;

static void
via_vt8371_log(const char *fmt, ...)
{
    va_list ap;

    if (via_vt8371_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define via_vt8371_log(fmt, ...)
#endif

typedef struct via_vt8371_t {
    uint8_t   drb_unit;
    uint8_t   pci_slot;
    uint8_t   pad;
    uint8_t   pad0;

    uint8_t   pci_conf[256];

    smram_t   *smram;
    agpgart_t *agpgart;
} via_vt8371_t;

static void
vt8371_map(uint32_t addr, uint32_t size, int state)
{
    switch (state & 3) {
        case 0:
            mem_set_mem_state_both(addr, size, MEM_READ_EXTANY | MEM_WRITE_EXTANY);
            break;
        case 1:
            mem_set_mem_state_both(addr, size, MEM_READ_EXTANY | MEM_WRITE_INTERNAL);
            break;
        case 2:
            mem_set_mem_state_both(addr, size, MEM_READ_INTERNAL | MEM_WRITE_EXTANY);
            break;
        case 3:
            mem_set_mem_state_both(addr, size, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
            break;
        default:
            break;
    }

    flushmmucache_nopc();
}

static void
vt8371_smram_map(via_vt8371_t *dev, int smm, uint32_t host_base, uint32_t size, int is_smram)
{
    if (((is_smram & 0x03) == 0x01) || ((is_smram & 0x03) == 0x02))
        smram_enable(dev->smram, host_base, 0x000a0000, size, 0, 1);

    mem_set_mem_state_smram_ex(smm, host_base, size, is_smram & 0x03);
    flushmmucache();
}

static void
vt8371_agp_map(via_vt8371_t *dev)
{
    /* Make sure the aperture's base is aligned to its size. */
    dev->pci_conf[0x12] &= dev->pci_conf[0x84] << 4;
    dev->pci_conf[0x13] &= 0xf0 | (dev->pci_conf[0x84] >> 4);

    if (!dev->agpgart)
        return;

    /* Map aperture and GART. */
    agpgart_set_aperture(dev->agpgart,
                         (dev->pci_conf[0x12] << 16) | (dev->pci_conf[0x13] << 24),
                         ((uint32_t) (uint8_t) ~dev->pci_conf[0x84] + 1) << 20,
                         !!(dev->pci_conf[0x88] & 0x02));
    agpgart_set_gart(dev->agpgart, (dev->pci_conf[0x89] << 8) | (dev->pci_conf[0x8a] << 16) | (dev->pci_conf[0x8b] << 24));
}

/*
 * Provenance note:
 * The VIA VT8371 (Apollo KX133) register description section was never published
 * by VIA in full. This driver is reconstructed from H. Oda's contemporary WPCREDIT
 * 11060391.PCR definition, cross-checked against the official VIA VT8363A (KT133A)
 * and VT82C694X (Apollo Pro133A) datasheets.
 */
static void
via_vt8371_setup(via_vt8371_t *dev)
{
    /* Host Bridge (1106:0391) */
    dev->pci_conf[0x00] = 0x06; /* Vendor ID: VIA (0x1106) */
    dev->pci_conf[0x01] = 0x11;
    dev->pci_conf[0x02] = 0x91; /* Device ID: VT8371 (0x0391) */
    dev->pci_conf[0x03] = 0x03;

    dev->pci_conf[0x04] = 0x06; /* Command: Memory + Bus Master */
    dev->pci_conf[0x05] = 0x00;

    dev->pci_conf[0x06] = 0x10; /* Status: Cap List */
    dev->pci_conf[0x07] = 0x02; /* Status: Medium DEVSEL */

    dev->pci_conf[0x08] = 0xc2; /* Revision ID: C2 */
    dev->pci_conf[0x09] = 0x00; /* Prog IF */
    dev->pci_conf[0x0a] = 0x00; /* Sub Class: Host Bridge */
    dev->pci_conf[0x0b] = 0x06; /* Base Class: Bridge Device */
    dev->pci_conf[0x0c] = 0x00;
    dev->pci_conf[0x0d] = 0x00;
    dev->pci_conf[0x0e] = 0x00;
    dev->pci_conf[0x0f] = 0x00;

    dev->pci_conf[0x10] = 0x08; /* Graphics Aperture Base default: 0x00000008 */
    dev->pci_conf[0x34] = 0xa0; /* Capability Pointer: AGP */

    dev->pci_conf[0x52] = 0x70; /* S2K Timing Control III */
    dev->pci_conf[0x56] = 0x01; /* Bank 6 ending */
    dev->pci_conf[0x57] = 0x01; /* Bank 7 ending */
    dev->pci_conf[0x58] = 0x40; /* MA Map Type */
    dev->pci_conf[0x59] = 0x00;
    dev->pci_conf[0x5a] = 0x01; /* Bank 0 ending */
    dev->pci_conf[0x5b] = 0x01; /* Bank 1 ending */
    dev->pci_conf[0x5c] = 0x01; /* Bank 2 ending */
    dev->pci_conf[0x5d] = 0x01; /* Bank 3 ending */
    dev->pci_conf[0x5e] = 0x01; /* Bank 4 ending */
    dev->pci_conf[0x5f] = 0x01; /* Bank 5 ending */

    dev->pci_conf[0x64] = 0xec; /* DRAM Timing banks 0,1 */
    dev->pci_conf[0x65] = 0xec; /* DRAM Timing banks 2,3 */
    dev->pci_conf[0x66] = 0xec; /* DRAM Timing banks 4,5 */
    dev->pci_conf[0x67] = 0xec; /* DRAM Timing banks 6,7 */

    /* Clocking: KX133 200 MHz FSB (100 MHz DDR), PCI 33 MHz, AGP 66 MHz */
    cpu_set_pci_speed(cpu_busspeed / 3);
    cpu_set_agp_speed(cpu_busspeed / 1.5);
    dev->pci_conf[0x68] |= 0x01; /* 100 MHz strap */
    dev->pci_conf[0x6b] = 0x01;  /* DRAM Arbitration Control */

    /* AGP Capability */
    dev->pci_conf[0xa0] = 0x02; /* AGP ID */
    dev->pci_conf[0xa1] = 0x00; /* AGP Next Item Pointer */
    dev->pci_conf[0xa2] = 0x20; /* AGP Spec Revision 2.0 */
    dev->pci_conf[0xa4] = 0x03; /* AGP Status: 1F000203 */
    dev->pci_conf[0xa5] = 0x02;
    dev->pci_conf[0xa6] = 0x00;
    dev->pci_conf[0xa7] = 0x1f;
    dev->pci_conf[0xad] = 0x02; /* AGP Latency Timer */
    dev->pci_conf[0xb0] = 0x80; /* AGP Pad Control / Status */
    dev->pci_conf[0xb1] = 0x63; /* AGP Drive Strength */

    /* Power Management Capability */
    dev->pci_conf[0xc0] = 0x01; /* PM Capability ID */
    dev->pci_conf[0xc1] = 0x00; /* PM Next Pointer */
    dev->pci_conf[0xc2] = 0x02; /* PM Capabilities I */
    dev->pci_conf[0xc3] = 0x00;

    dev->drb_unit = 16;
}

static void
via_vt8371_host_bridge_write(int func, int addr, UNUSED(int len), uint8_t val, void *priv)
{
    via_vt8371_t *dev = (via_vt8371_t *) priv;
    if (func)
        return;

    via_vt8371_log("VT8371: [W] %02X = %02X\n", addr, val);

    /* Read-only and reserved addresses */
    if ((addr < 4) || (addr == 6) || ((addr >= 8) && (addr < 0x0d)) ||
        ((addr >= 0x0e) && (addr < 0x12)) || ((addr >= 0x14) && (addr < 0x2c)) ||
        ((addr >= 0x30) && (addr < 0x50)) || (addr == 0x6f) ||
        ((addr >= 0x7c) && (addr < 0x7e)) || ((addr >= 0x81) && (addr < 0x84)) ||
        ((addr >= 0x85) && (addr < 0x88)) || ((addr >= 0x8c) && (addr < 0xa8)) ||
        ((addr >= 0xaa) && (addr < 0xac)) || (addr == 0xaf) ||
        ((addr >= 0xb3) && (addr < 0xc0)) || ((addr >= 0xc0) && (addr < 0xc4)) ||
        ((addr >= 0xc5) && (addr < 0xe0)) || ((addr >= 0xe1) && (addr < 0xf0)) ||
        ((addr >= 0xf9) && (addr < 0xfc)))
        return;

    switch (addr) {
        case 0x04:
            dev->pci_conf[0x04] = (dev->pci_conf[0x04] & ~0x40) | (val & 0x40);
            break;

        case 0x05:
            dev->pci_conf[0x05] = val;
            break;

        case 0x07:
            dev->pci_conf[0x07] &= ~(val & 0xb0);
            break;

        case 0x0d:
            dev->pci_conf[0x0d] = (dev->pci_conf[0x0d] & ~0xf8) | (val & 0xf8);
            dev->pci_conf[0x75] = (dev->pci_conf[0x75] & ~0x30) | ((val & 0x06) << 3);
            break;

        case 0x12: /* Graphics Aperture Base */
            dev->pci_conf[0x12] = val & 0xf0;
            vt8371_agp_map(dev);
            break;

        case 0x13: /* Graphics Aperture Base */
            dev->pci_conf[0x13] = val;
            vt8371_agp_map(dev);
            break;

        case 0x55:
            /* UNVERIFIED: Rx55[7:1] debug and S2K compensation CPU halt control. */
            dev->pci_conf[0x55] = val;
            break;

        case 0x56:
        case 0x57:
        case 0x5a:
        case 0x5b:
        case 0x5c:
        case 0x5d:
        case 0x5e:
        case 0x5f: /* DRAM Row Ending Address */
            dev->pci_conf[addr] = val;
            spd_write_drbs(dev->pci_conf, 0x5a, 0x56, dev->drb_unit);
            break;

        case 0x58:
            dev->pci_conf[0x58] = val;
            break;

        case 0x59:
            dev->pci_conf[0x59] = val;
            break;

        case 0x61: /* Shadow RAM Control 1 */
            /* UNVERIFIED: Rx61-63 shadow RAM control register layout taken from Pro133A/KT133A; omitted from PCR. */
            vt8371_map(0xc0000, 0x04000, val & 0x03);
            vt8371_map(0xc4000, 0x04000, (val & 0x0c) >> 2);
            vt8371_map(0xc8000, 0x04000, (val & 0x30) >> 4);
            vt8371_map(0xcc000, 0x04000, (val & 0xc0) >> 6);

            dev->pci_conf[0x61] = val;
            break;

        case 0x62: /* Shadow RAM Control 2 */
            vt8371_map(0xd0000, 0x04000, val & 0x03);
            vt8371_map(0xd4000, 0x04000, (val & 0x0c) >> 2);
            vt8371_map(0xd8000, 0x04000, (val & 0x30) >> 4);
            vt8371_map(0xdc000, 0x04000, (val & 0xc0) >> 6);

            dev->pci_conf[0x62] = val;
            break;

        case 0x63: /* Shadow RAM Control 3 */
            shadowbios       = 0;
            shadowbios_write = 0;

            vt8371_map(0xf0000, 0x10000, (val & 0x30) >> 4);
            shadowbios       = (((val & 0x30) >> 4) & 0x02);
            shadowbios_write = (((val & 0x30) >> 4) & 0x01);

            vt8371_map(0xe0000, 0x10000, (val & 0xc0) >> 6);
            shadowbios |= (((val & 0xc0) >> 6) & 0x02);
            shadowbios_write |= (((val & 0xc0) >> 6) & 0x01);

            /* UNVERIFIED: Rx63[3:2] memory hole not implemented. */
            dev->pci_conf[0x63] = val;
            smram_disable_all();
            /* UNVERIFIED: Rx63[1:0] semantics taken from VT82C694X; the VT8371 datasheet register section does not survive. */
            switch (val & 0x03) {
                default:
                case 0x00:
                    vt8371_smram_map(dev, 1, 0x000a0000, 0x00020000, 1); /* SMM: Code DRAM, Data DRAM */
                    vt8371_smram_map(dev, 0, 0x000a0000, 0x00020000, 0); /* Non-SMM: Code PCI, Data PCI */
                    break;
                case 0x01:
                    vt8371_smram_map(dev, 1, 0x000a0000, 0x00020000, 1); /* SMM: Code DRAM, Data DRAM */
                    vt8371_smram_map(dev, 0, 0x000a0000, 0x00020000, 1); /* Non-SMM: Code DRAM, Data DRAM */
                    break;
                case 0x02:
                    vt8371_smram_map(dev, 1, 0x000a0000, 0x00020000, 3); /* SMM: Code Invalid, Data Invalid */
                    vt8371_smram_map(dev, 0, 0x000a0000, 0x00020000, 2); /* Non-SMM: Code DRAM, Data PCI */
                    break;
                case 0x03:
                    vt8371_smram_map(dev, 1, 0x000a0000, 0x00020000, 1); /* SMM: Code DRAM, Data DRAM */
                    vt8371_smram_map(dev, 0, 0x000a0000, 0x00020000, 3); /* Non-SMM: Code Invalid, Data Invalid */
                    break;
            }
            break;

        case 0x68:
            dev->pci_conf[0x68] = (dev->pci_conf[0x68] & ~0xfc) | (val & 0xfc);
            break;

        case 0x69:
            dev->pci_conf[0x69] = (dev->pci_conf[0x69] & ~0xfe) | (val & 0xfe);
            break;

        case 0x6c:
            dev->pci_conf[0x6c] = (dev->pci_conf[0x6c] & ~0x1f) | (val & 0x1f);
            break;

        case 0x6d:
            dev->pci_conf[0x6d] = val;
            break;

        case 0x6e:
            dev->pci_conf[0x6e] = (dev->pci_conf[0x6e] & ~0xbf) | (val & 0xbf);
            break;

        case 0x70:
            dev->pci_conf[0x70] = (dev->pci_conf[0x70] & ~0xdf) | (val & 0xdf);
            break;

        case 0x71:
            dev->pci_conf[0x71] = (dev->pci_conf[0x71] & ~0xdf) | (val & 0xdf);
            break;

        case 0x73:
            dev->pci_conf[0x73] = (dev->pci_conf[0x73] & ~0x7f) | (val & 0x7f);
            break;

        case 0x74:
            dev->pci_conf[0x74] = (dev->pci_conf[0x74] & ~0xdf) | (val & 0xdf);
            break;

        case 0x75:
            dev->pci_conf[0x75] = (dev->pci_conf[0x75] & ~0xcf) | (val & 0xcf);
            break;

        case 0x76:
            /* UNVERIFIED: Rx76 */
            dev->pci_conf[0x76] = val;
            break;

        case 0x78:
            dev->pci_conf[0x78] = (dev->pci_conf[0x78] & ~0xd5) | (val & 0xd5);
            break;

        case 0x79:
            /* UNVERIFIED: Rx79 */
            dev->pci_conf[0x79] = (dev->pci_conf[0x79] & ~0xfc) | (val & 0xfc);
            break;

        case 0x7a:
            dev->pci_conf[0x7a] = (dev->pci_conf[0x7a] & ~0x99) | (val & 0x99);
            break;

        case 0x7b:
            dev->pci_conf[0x7b] = val & 0x01;
            break;

        case 0x7e:
            dev->pci_conf[0x7e] = (dev->pci_conf[0x7e] & ~0x3f) | (val & 0x3f);
            break;

        case 0x7f:
            dev->pci_conf[0x7f] = val;
            break;

        case 0x80:
            /* UNVERIFIED: Rx80[15:8] */
            dev->pci_conf[0x80] = (dev->pci_conf[0x80] & ~0x8f) | (val & 0x8f);
            break;

        case 0x84:
            dev->pci_conf[0x84] = val;
            vt8371_agp_map(dev);
            break;

        case 0x88:
            dev->pci_conf[0x88] = (dev->pci_conf[0x88] & ~0x06) | (val & 0x06);
            vt8371_agp_map(dev);
            break;

        case 0x89:
            dev->pci_conf[0x89] = val & 0xf0;
            vt8371_agp_map(dev);
            break;

        case 0x8a:
        case 0x8b:
            dev->pci_conf[addr] = val;
            vt8371_agp_map(dev);
            break;

        case 0xa8:
            dev->pci_conf[0xa8] = (dev->pci_conf[0xa8] & ~0x37) | (val & 0x37);
            break;

        case 0xa9:
            dev->pci_conf[0xa9] = (dev->pci_conf[0xa9] & ~0x03) | (val & 0x03);
            break;

        case 0xac:
            /* UNVERIFIED: RxAC */
            dev->pci_conf[0xac] = (dev->pci_conf[0xac] & ~0x7f) | (val & 0x7f);
            break;

        case 0xad:
            /* UNVERIFIED: RxAD */
            dev->pci_conf[0xad] = (dev->pci_conf[0xad] & ~0x1f) | (val & 0x1f);
            break;

        case 0xae:
            dev->pci_conf[0xae] = (dev->pci_conf[0xae] & ~0x34) | (val & 0x34);
            break;

        case 0xb0:
            /* UNVERIFIED: RxB0 */
            dev->pci_conf[0xb0] = (dev->pci_conf[0xb0] & ~0xc0) | (val & 0xc0);
            break;

        case 0xb1:
            dev->pci_conf[0xb1] = val;
            break;

        case 0xb2:
            /* UNVERIFIED: RxB2 */
            dev->pci_conf[0xb2] = (dev->pci_conf[0xb2] & ~0x93) | (val & 0x93);
            break;

        case 0xc4:
            dev->pci_conf[0xc4] = (dev->pci_conf[0xc4] & ~0x03) | (val & 0x03);
            break;

        case 0xe0:
            dev->pci_conf[0xe0] = (dev->pci_conf[0xe0] & ~0x01) | (val & 0x01);
            break;

        case 0xfd:
            dev->pci_conf[0xfd] = (dev->pci_conf[0xfd] & ~0x1f) | (val & 0x1f);
            break;

        default:
            dev->pci_conf[addr] = val;
            break;
    }
}

static uint8_t
via_vt8371_read(int func, int addr, UNUSED(int len), void *priv)
{
    const via_vt8371_t *dev = (via_vt8371_t *) priv;
    uint8_t             ret = 0xff;

    switch (func) {
        case 0:
            ret = dev->pci_conf[addr];
            break;
        default:
            break;
    }

    if (addr >= 0x50)
        via_vt8371_log("VT8371: [R] %02X = %02X\n", addr, ret);

    return ret;
}

static void
via_vt8371_write(int func, int addr, int len, uint8_t val, void *priv)
{
    switch (func) {
        case 0:
            via_vt8371_host_bridge_write(func, addr, len, val, priv);
            break;
        default:
            break;
    }
}

static void
via_vt8371_reset(void *priv)
{
    via_vt8371_write(0, 0x61, 1, 0x00, priv);
    via_vt8371_write(0, 0x62, 1, 0x00, priv);
    via_vt8371_write(0, 0x63, 1, 0x00, priv);
}

static void *
via_vt8371_init(UNUSED(const device_t *info))
{
    via_vt8371_t *dev = (via_vt8371_t *) calloc(1, sizeof(via_vt8371_t));

    dev->smram = smram_add();
    vt8371_smram_map(dev, 1, 0x000a0000, 0x00020000, 1); /* SMM: Code DRAM, Data DRAM */

    pci_add_card(PCI_ADD_NORTHBRIDGE, via_vt8371_read, via_vt8371_write, dev, &dev->pci_slot);

    device_add(&via_vt8371_agp_device);

    dev->agpgart = device_add(&agpgart_device);

    via_vt8371_setup(dev);
    via_vt8371_reset(dev);

    return dev;
}

static void
via_vt8371_close(void *priv)
{
    via_vt8371_t *dev = (via_vt8371_t *) priv;

    smram_del(dev->smram);

    free(dev);
}

const device_t via_vt8371_device = {
    .name          = "VIA Apollo KX133",
    .internal_name = "via_vt8371",
    .flags         = DEVICE_PCI,
    .local         = 0,
    .init          = via_vt8371_init,
    .close         = via_vt8371_close,
    .reset         = via_vt8371_reset,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};