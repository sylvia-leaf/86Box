/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Implementation of Intel 82845 Brookdale MCH Bridge
 *
 * Authors: Tiseno100,
 *          Jasmine Iwanek, <jriwanek@gmail.com>
 *          Bachimus, <mszoopers@protonmail.com>
 *
 *          Copyright 2022      Tiseno100.
 *          Copyright 2022-2023 Jasmine Iwanek.
 *          Copyright 2026      Bachimus.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include "cpu.h"
#include "x86.h"
#include <86box/device.h>
#include <86box/plat_unused.h>

#include <86box/chipset.h>
#include <86box/mem.h>
#include <86box/agpgart.h>
#include <86box/pci.h>
#include <86box/smram.h>
#include <86box/spd.h>
#include <86box/i2c.h>

#ifdef ENABLE_INTEL_850_LOG
int intel_850_do_log = ENABLE_INTEL_850_LOG;
static void
intel_850_log(const char *fmt, ...)
{
    va_list ap;

    if (intel_850_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define intel_850_log(fmt, ...)
#endif

/*
 * The BIOS enumerates the RDRAM channel over the SIO chain: it broadcasts
 * DEVID 31 to every device, then repeatedly writes DEVID to whichever device
 * still answers as 31 and reads it back. The read that fails to match is how
 * it learns the device count, so a device must stop answering past the end.
 */
#define INTEL_850_RDRAM_MAX_DEVICES 31
#define INTEL_850_RDRAM_DEVICE_KB   (32 * 1024)

typedef struct intel_850_t {
    uint8_t    pci_conf[256];
    uint8_t    subsystem_locked[4];
    uint8_t    pci_slot;
    uint8_t    pad[2];
    uint32_t   tseg_base;
    uint32_t   tseg_size;
    smram_t   *c_segment;
    smram_t   *h_segment;
    smram_t   *tseg_segment;
    agpgart_t *agpgart;
    uint8_t    revision;
    uint8_t    pad2[3];
    uint8_t    rdram_devices;
    uint8_t    rdram_group;
    uint16_t   spd_status; /* bit 15 done, bit 13 error, low byte data */
    uint16_t   ctl_regs[0x20]; /* controller registers, below the device range */
    uint16_t   rdram_devid[INTEL_850_RDRAM_MAX_DEVICES];
    const device_t *agp_device;
} intel_850_t;

static uint32_t
intel_850_tom(const intel_850_t *dev)
{
    const uint16_t tom = (dev->pci_conf[0xc4] | (dev->pci_conf[0xc5] << 8)) & 0xfff0;

    return (uint32_t) tom << 16;
}

static uint16_t
intel_850_default_tom_reg(void)
{
    uint16_t tom = (mem_size >> 6) & 0xfff0;

    return (tom < 0x0100) ? 0x0100 : tom;
}

static void
intel_850_set_tom_reg(intel_850_t *dev, uint16_t tom)
{
    dev->pci_conf[0xc4] = tom & 0xff;
    dev->pci_conf[0xc5] = tom >> 8;
}

static void
intel_850_agp_aperture(intel_850_t *dev)
{
    uint32_t aperture_base;
    uint32_t aperture_size;
    uint64_t aperture_end;
    int      aperture_enable;

    dev->pci_conf[0x10] = 0x08;
    dev->pci_conf[0x11] = 0x00;

    aperture_base = (dev->pci_conf[0x13] << 24) | (dev->pci_conf[0x12] << 16);
    aperture_base &= (((uint32_t) (dev->pci_conf[0xb4] & 0x3f) << 22) | 0xf0000000);

    dev->pci_conf[0x12] = (aperture_base >> 16) & 0xff;
    dev->pci_conf[0x13] = (aperture_base >> 24) & 0xff;

    aperture_size   = ((uint32_t) ((~dev->pci_conf[0xb4] & 0x3f) + 1)) << 22;
    aperture_end    = (uint64_t) aperture_base + aperture_size;
    aperture_enable = !!(dev->pci_conf[0x51] & 0x02) &&
                      (aperture_base != 0) &&
                      (dev->agpgart->gart_base != 0) &&
                      (aperture_base >= intel_850_tom(dev)) &&
                      (aperture_end > aperture_base) &&
                      (aperture_end <= 0x100000000ULL);

    if (aperture_enable)
        intel_850_log("Intel 850 MCH: AGP aperture enabled at %08x, size %u MB\n",
                      aperture_base, aperture_size >> 20);
    else
        intel_850_log("Intel 850 MCH: AGP aperture disabled\n");

    agpgart_set_aperture(dev->agpgart, aperture_base, aperture_size, aperture_enable);
}

static void
intel_850_gart_table(intel_850_t *dev)
{
    const uint32_t agp_gart_base = (dev->pci_conf[0xbb] << 24) | (dev->pci_conf[0xba] << 16) |
                                   (dev->pci_conf[0xb9] << 8) | dev->pci_conf[0xb8];

    intel_850_log("Intel 850 MCH: AGP GART base updated to %08x\n", agp_gart_base);

    agpgart_set_gart(dev->agpgart, agp_gart_base);
    intel_850_agp_aperture(dev);
}

static void
intel_850_pam_recalc(int addr, uint8_t val)
{
    int region = 0xc0000 + ((addr - 0x5a) << 15);

    if (addr == 0x59)
        mem_set_mem_state_both(0xf0000, 0x10000,
                               ((val & 0x10) ? MEM_READ_INTERNAL : MEM_READ_EXTANY) |
                               ((val & 0x20) ? MEM_WRITE_INTERNAL : MEM_WRITE_EXTANY));
    else {
        mem_set_mem_state_both(region, 0x4000,
                               ((val & 0x01) ? MEM_READ_INTERNAL : MEM_READ_EXTANY) |
                               ((val & 0x02) ? MEM_WRITE_INTERNAL : MEM_WRITE_EXTANY));
        mem_set_mem_state_both(region + 0x4000, 0x4000,
                               ((val & 0x10) ? MEM_READ_INTERNAL : MEM_READ_EXTANY) |
                               ((val & 0x20) ? MEM_WRITE_INTERNAL : MEM_WRITE_EXTANY));
    }

    flushmmucache();
}

static void
intel_850_fdhc_recalc(intel_850_t *dev)
{
    if ((mem_size << 10) > 0xf00000) {
        mem_set_mem_state_both(0xf00000, 0x100000,
                               (dev->pci_conf[0x58] & 0x80) ?
                               (MEM_READ_EXTANY | MEM_WRITE_EXTANY) :
                               (MEM_READ_INTERNAL | MEM_WRITE_INTERNAL));
        flushmmucache();
    }
}

static void
intel_850_smram_recalc(intel_850_t *dev)
{
    const uint8_t smram      = dev->pci_conf[0x9d];
    const uint8_t esmramc    = dev->pci_conf[0x9e];
    const int     g_smrame   = !!(smram & 0x08);
    const int     d_open     = ((smram & 0x50) == 0x40);
    const int     d_closed   = !!(smram & 0x20);
    uint32_t      tom        = intel_850_tom(dev);

    if (dev->tseg_size != 0) {
        mem_set_mem_state_both(dev->tseg_base, dev->tseg_size, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
        dev->tseg_base = dev->tseg_size = 0;
    }

    smram_disable(dev->c_segment);
    smram_disable(dev->h_segment);
    smram_disable(dev->tseg_segment);

    if (tom == 0 || (tom > (mem_size << 10)))
        tom = mem_size << 10;

    if (g_smrame) {
        smram_enable(dev->c_segment, 0x000a0000, 0x000a0000, 0x20000, d_open, 1);

        if (d_closed)
            mem_set_mem_state_smram_ex(1, 0x000a0000, 0x20000, ACCESS_SMRAM_W);

        if (esmramc & 0x80)
            smram_enable(dev->h_segment, 0xfeda0000, 0x000a0000, 0x20000, d_open, 1);

        if ((esmramc & 0x01) && (tom != 0)) {
            dev->tseg_size = 1 << (17 + ((esmramc >> 1) & 0x03));

            if (tom >= dev->tseg_size) {
                dev->tseg_base = tom - dev->tseg_size;

                if (!d_open)
                    mem_set_mem_state(dev->tseg_base, dev->tseg_size, MEM_READ_EXTANY | MEM_WRITE_EXTANY);

                smram_enable(dev->tseg_segment, dev->tseg_base, dev->tseg_base, dev->tseg_size, d_open, 1);
            } else
                dev->tseg_base = dev->tseg_size = 0;
        }
    }

    flushmmucache();
}

/*
 * POST sums SPD byte 63h (device count) across the populated RIMM sockets to
 * work out how many devices a channel should hold, then requires the chain walk
 * to hand out exactly that many DEVIDs. Take the count from the same place so
 * the two can never disagree.
 */
static void
intel_850_rdram_recount(intel_850_t *dev)
{
    const uint8_t base  = 0x50 + (dev->rdram_group << 1);
    uint8_t       total = 0;

    for (uint8_t addr = base; addr <= (base + 1); addr++) {
        if (!i2c_start(i2c_smbus, addr, 0))
            continue;

        i2c_write(i2c_smbus, addr, 0x63);
        i2c_start(i2c_smbus, addr, 1);
        total += i2c_read(i2c_smbus, addr);
        i2c_stop(i2c_smbus, addr);
    }

    if (total > INTEL_850_RDRAM_MAX_DEVICES)
        total = INTEL_850_RDRAM_MAX_DEVICES;

    intel_850_log("Intel 850 MCH: socket pair %d holds %d RDRAM devices\n",
                  dev->rdram_group, total);

    dev->rdram_devices = total;
}

/* Devices are addressed by the low 5 bits of their current DEVID. */
static int
intel_850_rdram_find(const intel_850_t *dev, uint8_t devid)
{
    for (uint8_t i = 0; i < dev->rdram_devices; i++) {
        if ((dev->rdram_devid[i] & 0x1f) == (devid & 0x1f))
            return i;
    }

    return -1;
}

static void
intel_850_ricm_exec(intel_850_t *dev)
{
    const uint8_t  iop   = dev->pci_conf[0x94] & 0x1f;
    const uint8_t  devid = ((dev->pci_conf[0x95] & 0x03) << 3) | (dev->pci_conf[0x94] >> 5);
    const uint16_t reg   = ((dev->pci_conf[0x96] & 0x07) << 6) | (dev->pci_conf[0x95] >> 2);
    const int      bcast = !!(dev->pci_conf[0x96] & 0x08);
    const uint16_t data  = dev->pci_conf[0x90] | (dev->pci_conf[0x91] << 8);
    uint16_t       ret   = 0xffff; /* no device answers */
    int            i;

    intel_850_log("Intel 850 MCH: RICM opcode %02x, device %02x%s, reg %03x, data %04x\n",
                  iop, devid, bcast ? " (broadcast)" : "", reg, data);

    switch (iop) {
        case 0x00: /* SRD - serial read */
            /*
             * Registers below 020h live in the controller, not in an RDRAM
             * device, so they answer whichever socket pair is selected - a
             * device lookup here would report "nobody home" for an empty pair
             * and leave POST polling forever.
             */
            if (reg < 0x020) {
                /* POST writes these and reads them straight back. */
                ret = (reg == 0x00a) ? dev->spd_status : dev->ctl_regs[reg];

                dev->pci_conf[0x90] = dev->pci_conf[0x92] = ret & 0xff;
                dev->pci_conf[0x91] = dev->pci_conf[0x93] = ret >> 8;
                break;
            }

            i = intel_850_rdram_find(dev, devid);

            if (i >= 0) switch (reg) {
                case 0x21: /* DEVID */
                    ret = dev->rdram_devid[i];
                    break;

                case 0x23: /* device type - POST requires this exact value */
                    ret = 0x8001;
                    break;


                default:
                    ret = 0x0000;
                    break;
            }

            /* Interfaces A and B run in lockstep, so both halves read alike. */
            dev->pci_conf[0x90] = dev->pci_conf[0x92] = ret & 0xff;
            dev->pci_conf[0x91] = dev->pci_conf[0x93] = ret >> 8;
            break;

        case 0x01: /* SWR - serial write */
            if (reg < 0x020) { /* controller register, see the SRD case */
                /*
                 * 00fh commands a device power-state change: bit 3 is the busy
                 * flag hardware clears once the transition finishes and bit 4
                 * reports that it succeeded. POST spins on bit 3 and then
                 * insists on bit 4, so retire the request immediately.
                 */
                if (reg == 0x00f)
                    dev->ctl_regs[reg] = (data & ~0x0008) | 0x0010;
                else
                    dev->ctl_regs[reg] = data;
                break;
            }

            if (reg != 0x21) /* only DEVID changes anything we model */
                break;

            if (bcast) {
                for (i = 0; i < dev->rdram_devices; i++)
                    dev->rdram_devid[i] = data;
            } else {
                i = intel_850_rdram_find(dev, devid);

                if (i >= 0)
                    dev->rdram_devid[i] = data;
            }
            break;

        case 0x0e: /* start an SPD read - reg field carries the 7-bit slave address */
            /*
             * Once the device walk succeeds the BIOS reads RIMM SPD through the
             * MCH rather than the ICH2 SMBus, so bridge it onto the same bus the
             * SPD devices live on. The byte offset arrives in the data register.
             */
            if (i2c_start(i2c_smbus, reg, 0)) {
                i2c_write(i2c_smbus, reg, data & 0xff);
                i2c_start(i2c_smbus, reg, 1);
                dev->spd_status = 0x8000 | i2c_read(i2c_smbus, reg);
                i2c_stop(i2c_smbus, reg);
            } else
                dev->spd_status = 0xa000; /* done, but nothing answered */
            break;

        case 0x08: /* selects which RIMM socket pair the chain addresses */
            if (dev->rdram_group != devid) {
                dev->rdram_group = devid;
                intel_850_rdram_recount(dev);
            }
            break;

        case 0x09: /* broadcast reset - returns every device to unassigned */
            /*
             * POST re-runs the chain enumeration after the first pass, again
             * addressing DEVID 31 to reach the next unassigned device. Without
             * this the second pass writes into nothing and the readback fails.
             */
            for (i = 0; i < dev->rdram_devices; i++)
                dev->rdram_devid[i] = 0x001f;
            break;

        case 0x11: /* SIO reset - issued once before the chain walk begins */
            intel_850_rdram_recount(dev);

            for (i = 0; i < dev->rdram_devices; i++)
                dev->rdram_devid[i] = 0x001f;
            break;

        default: /* SIO reset, SETR/CLRR and friends need no state */
            break;
    }
}

static void
intel_850_write(int func, int addr, UNUSED(int len), uint8_t val, void *priv)
{
    intel_850_t *dev = (intel_850_t *) priv;
    uint16_t     reg;
    
    intel_850_log("Intel 850 MCH: dev->regs[%02x] = %02x\n", addr, val);

    if (func)
        return;

    switch (addr) {
        case 0x05:
            dev->pci_conf[addr] = val & 0x01;
            break;

        case 0x07:
            dev->pci_conf[addr] &= ~(val & 0x70);
            break;

        case 0x10:
        case 0x11:
            break;

        case 0x12:
        case 0x13:
            dev->pci_conf[addr] = val;
            intel_850_agp_aperture(dev);
            break;











        case 0x2c ... 0x2f:
            if (!dev->subsystem_locked[addr - 0x2c]) {
                dev->pci_conf[addr] = val;
                dev->subsystem_locked[addr - 0x2c] = 1;
            }
            break;


        case 0x40 ... 0x4f: /* RDRAM GARs */
            dev->pci_conf[addr] = val;
            spd_write_gar_gbar_intel_850(dev->pci_conf);
            break;








        case 0x50:
            dev->pci_conf[addr] = (dev->pci_conf[addr] & 0x04) | (val & 0xa2);
            break;

        case 0x51:
            dev->pci_conf[addr] = val & 0x0b;
            intel_850_agp_aperture(dev);
            break;

        case 0x58:
            dev->pci_conf[addr] = val & 0x80;
            intel_850_fdhc_recalc(dev);
            break;

        case 0x59:
            dev->pci_conf[addr] = val & 0x30;
            intel_850_pam_recalc(addr, dev->pci_conf[addr]);
            break;

        case 0x5a ... 0x5f:
            dev->pci_conf[addr] = val & 0x33;
            intel_850_pam_recalc(addr, dev->pci_conf[addr]);
            break;





        case 0x60 ... 0x7f: /* RDRAM GBARs */
            if (!(dev->pci_conf[0x9d] & 0x10)) {
                if (addr & 1)
                    dev->pci_conf[addr] = val & 0x3f;
                else
                    dev->pci_conf[addr] = val;
            
                spd_write_gar_gbar_intel_850(dev->pci_conf);
            }
            break;





        case 0x88: /* RDRAM Pool Sizing Register */
            if (!(dev->pci_conf[0x88] & 0x80)) {
                dev->pci_conf[0x88] = val & ~0x20;
            }
            break;

        case 0x90 ... 0x93: /* RDRAM Device Register Data Register */
            dev->pci_conf[addr] = val;
            break;

        case 0x94: /* RDRAM Initialisation Control Management Register */
        case 0x95: 
            dev->pci_conf[addr] = val;
            break;
        case 0x96:
            dev->pci_conf[addr] = val & 0xbf;
            /*
            * IIO (bit 23) launches the opcode in IOP. Hardware clears it when the
            * operation completes and the BIOS polls for that, so complete it inline.
            * Every field the operation consumes (IOP, SDCA, DRA, BA, CID) lives in
            * 94-96h, so this byte is the point at which the request is complete.
            */
            if (dev->pci_conf[addr] & 0x80) {
                intel_850_ricm_exec(dev);
                dev->pci_conf[addr] &= ~0x80;
            }
            break;
        case 0x97: /* RICM byte 3 - BIOS reads bit 24 (bit 0 here) back */
            dev->pci_conf[addr] = val;
            break;
        case 0x9d:
            if (dev->pci_conf[0x9d] & 0x10) {
                dev->pci_conf[0x9d] = (dev->pci_conf[0x9d] & ~0x20) | (val & 0x20) | 0x02;
            } else {
                dev->pci_conf[0x9d] = (val & 0x78) | 0x02;
                if (dev->pci_conf[0x9d] & 0x10)
                    dev->pci_conf[0x9d] &= ~0x40;
            }
            intel_850_smram_recalc(dev);
            break;

        case 0x9e:
            if (dev->pci_conf[0x9d] & 0x10)
                dev->pci_conf[0x9e] = (dev->pci_conf[0x9e] & ~(val & 0x40)) | 0x38;
            else
                dev->pci_conf[0x9e] = ((dev->pci_conf[0x9e] & 0x40) & ~(val & 0x40)) | (val & 0x87) | 0x38;
            intel_850_smram_recalc(dev);
            break;

        case 0xa8:
            dev->pci_conf[addr] = val & 0x17;
            break;

        case 0xa9:
            dev->pci_conf[addr] = val & 0x03;
            break;

        case 0xaa:
        case 0xab:
            dev->pci_conf[addr] = 0x00;
            break;

        case 0xb0:
            dev->pci_conf[addr] = val & 0x81;
            break;

        case 0xb1 ... 0xb3:
            dev->pci_conf[addr] = 0x00;
            break;

        case 0xb4:
            dev->pci_conf[addr] = val & 0x3f;
            intel_850_agp_aperture(dev);
            break;

        case 0xb8:
            dev->pci_conf[addr] = 0x00;
            intel_850_gart_table(dev);
            break;

        case 0xb9:
            dev->pci_conf[addr] = val & 0xf0;
            intel_850_gart_table(dev);
            break;

        case 0xba:
        case 0xbb:
            dev->pci_conf[addr] = val;
            intel_850_gart_table(dev);
            break;

        case 0xbc:
        case 0xbd:
            dev->pci_conf[addr] = val & 0xf8;
            break;

        case 0xbe: /* RDRAM Timing Register */
            dev->pci_conf[addr] = val & 0xdf;
            break;




        case 0xc4:
            dev->pci_conf[addr] = val & 0xf0;
            intel_850_smram_recalc(dev);
            intel_850_agp_aperture(dev);
            break;

        case 0xc5:
            dev->pci_conf[addr] = val;
            intel_850_smram_recalc(dev);
            intel_850_agp_aperture(dev);
            break;







        case 0xc8:
        case 0xc9:
            dev->pci_conf[addr] &= ~val;
            break;

        case 0xca:
            dev->pci_conf[addr] = val & 0x7f;
            break;

        case 0xcb:
            dev->pci_conf[addr] = val & 0x02;
            break;

        case 0xcc:
        case 0xce:
            dev->pci_conf[addr] = val & 0x03;
            break;

        case 0xcd:
        case 0xcf:
            dev->pci_conf[addr] = 0x00;
            break;






        case 0xdc: /* RDRAM Refresh Control Registers */
            dev->pci_conf[addr] = val & 0x07;
            break;
        case 0xdd:
            dev->pci_conf[addr] = 0x00;
            break;


        case 0xde:
        case 0xdf:
            dev->pci_conf[addr] = val;
            break;


        default:
            break;
    }

    reg = dev->pci_conf[0xc4] | (dev->pci_conf[0xc5] << 8);
    if ((addr == 0xc4 || addr == 0xc5) && (reg < 0x0100)) {
        intel_850_set_tom_reg(dev, intel_850_default_tom_reg());
        intel_850_smram_recalc(dev);
        intel_850_agp_aperture(dev);
    }
}

static uint8_t
intel_850_read(int func, int addr, UNUSED(int len), void *priv)
{
    const intel_850_t *dev = (intel_850_t *) priv;
    uint8_t            ret;

    if (func)
        ret = 0xff;
    else if ((addr == 0xa4) && (dev->pci_conf[0xb0] & 0x01))
        ret = (dev->pci_conf[addr] & ~0x07) | 0x01;
    else
        ret = dev->pci_conf[addr];

    intel_850_log("Intel 850 MCH: dev->regs[%02x] (%02x)\n", addr, ret);

    return ret;
}

static void
intel_850_reset(void *priv)
{
    intel_850_t *dev = (intel_850_t *) priv;

    if (dev->tseg_size != 0) {
        mem_set_mem_state_both(dev->tseg_base, dev->tseg_size, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
        dev->tseg_base = dev->tseg_size = 0;
    }

    memset(dev->pci_conf, 0x00, sizeof(dev->pci_conf));




    memset(dev->subsystem_locked, 0x00, sizeof(dev->subsystem_locked));

    /* Two channels in lockstep, so a channel holds half the installed memory. */
    dev->rdram_devices = mem_size / (2 * INTEL_850_RDRAM_DEVICE_KB);
    if (dev->rdram_devices < 1)
        dev->rdram_devices = 1;
    else if (dev->rdram_devices > INTEL_850_RDRAM_MAX_DEVICES)
        dev->rdram_devices = INTEL_850_RDRAM_MAX_DEVICES;

    for (uint8_t i = 0; i < INTEL_850_RDRAM_MAX_DEVICES; i++)
        dev->rdram_devid[i] = 0x001f; /* unassigned until POST walks the chain */

    dev->pci_conf[0x00] = 0x86; /* VID - Intel */
    dev->pci_conf[0x01] = 0x80;
    dev->pci_conf[0x02] = 0x30; /* DID - 82850 MCH */
    dev->pci_conf[0x03] = 0x25;
    dev->pci_conf[0x04] = 0x06; /* PCICMD */
    dev->pci_conf[0x06] = 0x90; /* PCISTS */
    dev->pci_conf[0x08] = dev->revision; /* RID */
    dev->pci_conf[0x0b] = 0x06; /* BCC - bridge */
    dev->pci_conf[0x10] = 0x08; /* APBASE */
    dev->pci_conf[0x34] = 0xa0; /* CAPPTR */
    memset(&dev->pci_conf[0x40], 0x80, 16); /* GAR0-15 default to 0x80 */
    for (int i = 0x60; i <= 0x7e; i += 2) {
        dev->pci_conf[i]     = 0x01; /* Default = 0x0001 (16 MB initial boundary) */
        dev->pci_conf[i + 1] = 0x00;
    }
    dev->pci_conf[0x9d] = 0x02; /* SMRAM */
    dev->pci_conf[0x9e] = 0x38; /* ESMRAMC */
    dev->pci_conf[0xa0] = 0x02; /* ACAPID */
    dev->pci_conf[0xa2] = 0x20;
    dev->pci_conf[0xa4] = 0x17; /* AGPSTAT */
    dev->pci_conf[0xa5] = 0x02;
    dev->pci_conf[0xa7] = 0x1f;
    intel_850_set_tom_reg(dev, intel_850_default_tom_reg()); /* TOM */

    uint16_t mapped_mb = spd_write_gar_gbar_intel_850(dev->pci_conf); /* GARs and GBARs */
    if (mapped_mb > 0) {
        uint16_t tom = (mapped_mb << 6) & 0xfff0;
        intel_850_set_tom_reg(dev, (tom < 0x0100) ? 0x0100 : tom);
    }



    intel_850_agp_aperture(dev);
    intel_850_gart_table(dev);




    for (int i = 0x59; i <= 0x5f; i++)
        intel_850_pam_recalc(i, dev->pci_conf[i]);

    intel_850_fdhc_recalc(dev);
    intel_850_smram_recalc(dev);
}

static void
intel_850_close(void *priv)
{
    intel_850_t *dev = (intel_850_t *) priv;

    smram_del(dev->c_segment);
    smram_del(dev->h_segment);
    smram_del(dev->tseg_segment);





    free(dev);
}

static void *
intel_850_init(const device_t *info)
{
    intel_850_t *dev = (intel_850_t *) calloc(1, sizeof(intel_850_t));

    dev->revision   = 0x02;
    dev->agp_device = &intel_850_agp_device;

    cpu_set_pci_speed(33333333);
    cpu_set_agp_speed(66666667);

    pci_add_card(PCI_ADD_NORTHBRIDGE, intel_850_read, intel_850_write, dev, &dev->pci_slot);

    device_add(dev->agp_device);
    dev->agpgart = device_add(&agpgart_device);






    cpu_cache_int_enabled = 1;
    cpu_cache_ext_enabled = 1;
    cpu_update_waitstates();

    dev->c_segment    = smram_add();
    dev->h_segment    = smram_add();
    dev->tseg_segment = smram_add();

    intel_850_reset(dev);
    return dev;
}

const device_t intel_850_device = {
    .name          = "Intel 850 MCH Bridge",
    .internal_name = "intel_850",
    .flags         = DEVICE_PCI,
    .local         = 0,
    .init          = intel_850_init,
    .close         = intel_850_close,
    .reset         = intel_850_reset,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};