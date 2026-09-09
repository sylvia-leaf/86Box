/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Emulation of SPD (Serial Presence Detect) devices.
 *
 * Authors: RichardG, <richardg867@gmail.com>
 *
 *          Copyright 2020 RichardG.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include <86box/device.h>
#include <86box/i2c.h>
#include <86box/spd.h>
#include <86box/version.h>
#include <86box/machine.h>
#include <86box/plat_unused.h>

#define SPD_ROLLUP(x) ((x) >= 16 ? ((x) -15) : (x))

uint8_t spd_present = 0;
spd_t  *spd_modules[SPD_MAX_SLOTS];

static const device_t spd_device;

#ifdef ENABLE_SPD_LOG
int spd_do_log = ENABLE_SPD_LOG;

static void
spd_log(const char *fmt, ...)
{
    va_list ap;

    if (spd_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define spd_log(fmt, ...)
#endif

static void
spd_close(UNUSED(void *priv))
{
    spd_log("SPD: close()\n");

    for (uint8_t i = 0; i < SPD_MAX_SLOTS; i++) {
        if (spd_modules[i])
            i2c_eeprom_close(spd_modules[i]->eeprom);
    }

    spd_present = 0;
}

static void *
spd_init(UNUSED(const device_t *info))
{
    spd_log("SPD: init()\n");

    for (uint8_t i = 0; i < SPD_MAX_SLOTS; i++) {
        if (spd_modules[i])
            spd_modules[i]->eeprom = i2c_eeprom_init(i2c_smbus, SPD_BASE_ADDR + i, spd_modules[i]->data, sizeof(spd_modules[i]->data), 0);
    }

    spd_present = 1;

    return &spd_modules;
}

int
comp_ui16_rev(const void *elem1, const void *elem2)
{
    const uint16_t a = *((const uint16_t *) elem1);
    const uint16_t b = *((const uint16_t *) elem2);
    return ((a > b) ? -1 : ((a < b) ? 1 : 0));
}

void
spd_populate(uint16_t *rows, uint8_t slot_count, uint16_t total_size, uint16_t min_module_size, uint16_t max_module_size, uint8_t enable_asym)
{
    uint8_t  row;
    uint8_t  next_empty_row;
    uint8_t  split;
    uint16_t asym;

    /* Populate rows with modules in power-of-2 capacities. */
    memset(rows, 0, SPD_MAX_SLOTS << 1);
    for (row = 0; row < slot_count && total_size; row++) {
        /* populate slot */
        rows[row] = 1 << log2i(MIN(total_size, max_module_size));
        if (total_size >= rows[row]) {
            spd_log("SPD: Initial row %d = %d MB\n", row, rows[row]);
            total_size -= rows[row];
        } else {
            rows[row] = 0;
            break;
        }
    }

    /* Did we populate all the RAM? */
    if (total_size) {
        /* Work backwards to add the missing RAM as asymmetric modules if possible. */
        if (enable_asym) {
            row = slot_count - 1;
            do {
                asym = (1 << log2i(MIN(total_size, rows[row])));
                if (rows[row] + asym <= max_module_size) {
                    rows[row] += asym;
                    total_size -= asym;
                }
            } while ((row-- > 0) && total_size);
        }

        if (total_size) /* still not enough */
            spd_log("SPD: Not enough RAM slots (%d) to cover memory (%d MB short)\n", slot_count, total_size);
    }

    /* Populate empty rows by splitting modules... */
    split = (total_size == 0); /* ...if possible. */
    while (split) {
        /* Look for a module to split. */
        split = 0;
        for (row = 0; row < slot_count; row++) {
            if ((rows[row] <= (min_module_size << 1)) || (rows[row] != (1 << log2i(rows[row]))))
                continue; /* no module here, module is too small to be split, or asymmetric module */

            /* Find next empty row. */
            next_empty_row = 0;
            for (uint8_t i = row + 1; i < slot_count && !next_empty_row; i++) {
                if (!rows[i])
                    next_empty_row = i;
            }
            if (!next_empty_row)
                break; /* no empty rows left */

            /* Split the module into its own row and the next empty row. */
            spd_log("SPD: splitting row %d (%d MB) into %d and %d (%d MB each)\n", row, rows[row], row, next_empty_row, rows[row] >> 1);
            rows[row] = rows[next_empty_row] = rows[row] >> 1;
            split                            = 1;
            break;
        }

        /* Sort rows by descending capacity if any were split. */
        if (split)
            qsort(rows, slot_count, sizeof(uint16_t), comp_ui16_rev);
    }
}

/* Populate one RDRAM channel, RIMMs must be matched between channels, the output is copied to all channels */
void
spd_populate_rdram(uint16_t *rimm_configs, uint8_t slots_per_channel, uint16_t channel_size, uint16_t max_module_size, uint8_t rdram_device_mask)
{
    
    memset(rimm_configs, 0, SPD_MAX_SLOTS << 1);


    for (uint8_t channel_slot = 0; (channel_slot < slots_per_channel); channel_slot++) {
        uint8_t device_size;

        if ((rdram_device_mask & SPD_RDRAM_256MBIT) && (channel_size >= 32))
            device_size = SPD_RDRAM_256MBIT;
        else if ((rdram_device_mask & SPD_RDRAM_128MBIT) && (channel_size >= 16))
            device_size = SPD_RDRAM_128MBIT;
        else if ((rdram_device_mask & SPD_RDRAM_64MBIT) && (channel_size >= 8))
            device_size = SPD_RDRAM_64MBIT;
        else
            break;

        uint16_t max_usable_rimm  = MIN(max_module_size, device_size * 16);
        uint8_t  device_count     = MIN(channel_size, max_usable_rimm) / device_size;

        if (device_count == 0)
            break;

        uint16_t target_rimm_size = device_count * device_size;

        rimm_configs[channel_slot] = (device_size << 8) | device_count;
        channel_size -= target_rimm_size;

        spd_log("SPD: RIMM channel slot %d = %d MB (%d x %d-Mbit devices)\n",
            channel_slot, target_rimm_size, device_count, device_size * 8);
    }

    if (channel_size)
        spd_log("SPD: Not enough RIMM slots in channel (%d) to cover memory (%d MB short)\n",
            slots_per_channel, channel_size);
}

static int
spd_write_part_no(char *part_no, char *type, uint16_t size)
{
    char size_unit;

    if (size >= 1024) {
        size_unit = 'G';
        size >>= 10;
    } else {
        size_unit = 'M';
    }

    return sprintf(part_no, EMU_NAME "-%s-%03d%c", type, size, size_unit);
}

void
spd_register(uint8_t ram_type, uint8_t slot_mask, uint16_t max_module_size)
{
    uint8_t      slot;
    uint8_t      slot_count;
    uint8_t      row;
    uint16_t     min_module_size;
    uint16_t     rows[SPD_MAX_SLOTS];
    uint16_t     asym;
    spd_edo_t   *edo_data;
    spd_sdram_t *sdram_data;

    /* Determine the minimum module size for this RAM type. */
    switch (ram_type) {
        case SPD_TYPE_FPM:
        case SPD_TYPE_EDO:
            min_module_size = SPD_MIN_SIZE_EDO;
            break;

        case SPD_TYPE_SDRAM:
            min_module_size = SPD_MIN_SIZE_SDRAM;
            break;

        default:
            spd_log("SPD: unknown RAM type %02X\n", ram_type);
            return;
    }

    /* Count how many slots are enabled. */
    slot_count = 0;
    for (slot = 0; slot < SPD_MAX_SLOTS; slot++) {
        rows[slot] = 0;
        if (slot_mask & (1 << slot))
            slot_count++;
    }

    /* Populate rows. */
    spd_populate(rows, slot_count, (mem_size >> 10), min_module_size, max_module_size, 1);

    /* Register SPD devices and populate their data according to the rows. */
    row = 0;
    for (slot = 0; (slot < SPD_MAX_SLOTS) && rows[row]; slot++) {
        if (!(slot_mask & (1 << slot)))
            continue; /* slot disabled */

        spd_modules[slot] = (spd_t *) calloc(1, sizeof(spd_t));
        spd_modules[slot]->slot = slot;
        spd_modules[slot]->size = rows[row];

        /* Determine the second row size, from which the first row size can be obtained. */
        asym = rows[row] - (1 << log2i(rows[row])); /* separate the powers of 2 */
        if (!asym)                                  /* is the module asymmetric? */
            asym = rows[row] >> 1;                  /* symmetric, therefore divide by 2 */

        spd_modules[slot]->row1 = rows[row] - asym;
        spd_modules[slot]->row2 = asym;

        spd_log("SPD: Registering slot %d = row %d = %d MB (%d/%d)\n", slot, row, rows[row], spd_modules[slot]->row1, spd_modules[slot]->row2);

        switch (ram_type) {
            case SPD_TYPE_FPM:
            case SPD_TYPE_EDO:
                edo_data = &spd_modules[slot]->edo_data;

                /* EDO SPD is specified by JEDEC and present in some modules, but
                   most utilities cannot interpret it correctly. SIV32 at least gets
                   the module capacities right, so it was used as a reference here. */
                edo_data->bytes_used = 0x80;
                edo_data->spd_size   = 0x08;
                edo_data->mem_type   = ram_type;
                edo_data->row_bits   = SPD_ROLLUP(7 + log2i(spd_modules[slot]->row1)); /* first row */
                edo_data->col_bits   = 9;
                if (spd_modules[slot]->row1 != spd_modules[slot]->row2) {                      /* the upper 4 bits of row_bits/col_bits should be 0 on a symmetric module */
                    edo_data->row_bits |= SPD_ROLLUP(7 + log2i(spd_modules[slot]->row2)) << 4; /* second row, if different from first */
                    edo_data->col_bits |= 9 << 4;                                              /* same as first row, but just in case */
                }
                edo_data->banks          = 2;
                edo_data->data_width_lsb = 64;
                edo_data->signal_level   = SPD_SIGNAL_LVTTL;
                edo_data->trac           = 50;
                edo_data->tcac           = 13;
                edo_data->refresh_rate   = SPD_REFRESH_NORMAL;
                edo_data->dram_width     = 8;

                edo_data->spd_rev = 0x12;
                for (int i = spd_write_part_no(edo_data->part_no, (ram_type == SPD_TYPE_FPM) ? "FPM" : "EDO", rows[row]);
                     i < sizeof(edo_data->part_no); i++)
                    edo_data->part_no[i] = ' '; /* part number should be space-padded */
                edo_data->rev_code[0] = BCD8(EMU_VERSION_MAJ);
                edo_data->rev_code[1] = BCD8(EMU_VERSION_MIN);
                edo_data->mfg_year    = 20;
                edo_data->mfg_week    = 17;

                for (uint8_t i = 0; i < 63; i++)
                    edo_data->checksum += spd_modules[slot]->data[i];
                for (uint8_t i = 0; i < 129; i++)
                    edo_data->checksum2 += spd_modules[slot]->data[i];
                break;

            case SPD_TYPE_SDRAM:
                sdram_data = &spd_modules[slot]->sdram_data;

                sdram_data->bytes_used = 0x80;
                sdram_data->spd_size   = 0x08;
                sdram_data->mem_type   = ram_type;
                sdram_data->row_bits   = SPD_ROLLUP(6 + log2i(spd_modules[slot]->row1)); /* first row */
                sdram_data->col_bits   = 9;
                if (spd_modules[slot]->row1 != spd_modules[slot]->row2) {                        /* the upper 4 bits of row_bits/col_bits should be 0 on a symmetric module */
                    sdram_data->row_bits |= SPD_ROLLUP(6 + log2i(spd_modules[slot]->row2)) << 4; /* second row, if different from first */
                    sdram_data->col_bits |= 9 << 4;                                              /* same as first row, but just in case */
                }
                sdram_data->rows           = 2;
                sdram_data->data_width_lsb = 64;
                sdram_data->signal_level   = SPD_SIGNAL_LVTTL;
                sdram_data->tclk           = 0x75; /* 7.5 ns = 133.3 MHz */
                sdram_data->tac            = 0x10;
                sdram_data->refresh_rate   = SPD_SDR_REFRESH_SELF | SPD_REFRESH_NORMAL;
                sdram_data->sdram_width    = 8;
                sdram_data->tccd           = 1;
                sdram_data->burst          = SPD_SDR_BURST_PAGE | 1 | 2 | 4 | 8;
                sdram_data->banks          = 4;
                sdram_data->cas            = 0x1c; /* CAS 5/4/3 supported */
                sdram_data->cslat = sdram_data->we = 0x7f;
                sdram_data->dev_attr               = SPD_SDR_ATTR_EARLY_RAS | SPD_SDR_ATTR_AUTO_PC | SPD_SDR_ATTR_PC_ALL | SPD_SDR_ATTR_W1R_BURST;
                sdram_data->tclk2                  = 0xA0; /* 10 ns = 100 MHz */
                sdram_data->tclk3                  = 0xF0; /* 15 ns = 66.7 MHz */
                sdram_data->tac2 = sdram_data->tac3 = 0x10;
                sdram_data->trp = sdram_data->trrd = sdram_data->trcd = sdram_data->tras = 1;
                if (spd_modules[slot]->row1 != spd_modules[slot]->row2) {
                    /* Utilities interpret bank_density a bit differently on asymmetric modules. */
                    sdram_data->bank_density = 1 << (log2i(spd_modules[slot]->row1 >> 1) - 2);  /* first row */
                    sdram_data->bank_density |= 1 << (log2i(spd_modules[slot]->row2 >> 1) - 2); /* second row */
                } else {
                    sdram_data->bank_density = 1 << (log2i(spd_modules[slot]->row1 >> 1) - 1); /* symmetric module = only one bit is set */
                }
                sdram_data->ca_setup = sdram_data->data_setup = 0x15;
                sdram_data->ca_hold = sdram_data->data_hold = 0x08;

                sdram_data->spd_rev = 0x12;
                for (int i = spd_write_part_no(sdram_data->part_no, "SDR", rows[row]);
                     i < sizeof(sdram_data->part_no); i++)
                    sdram_data->part_no[i] = ' '; /* part number should be space-padded */
                sdram_data->rev_code[0] = BCD8(EMU_VERSION_MAJ);
                sdram_data->rev_code[1] = BCD8(EMU_VERSION_MIN);
                sdram_data->mfg_year    = 20;
                sdram_data->mfg_week    = 13;

                sdram_data->freq     = 100;
                sdram_data->features = 0xFF;

                for (uint8_t i = 0; i < 63; i++)
                    sdram_data->checksum += spd_modules[slot]->data[i];
                for (uint8_t i = 0; i < 129; i++)
                    sdram_data->checksum2 += spd_modules[slot]->data[i];
                break;

            default:
                break;
        }

        row++;
    }

    device_add(&spd_device);
}

void
spd_register_rdram(uint8_t slot_mask, uint8_t channel_count, uint16_t max_module_size, uint8_t rdram_device_mask)
{
    uint16_t     rimm_configs[SPD_MAX_SLOTS];
    spd_rdram_t *rdram_data;

    /* Count how many slots are enabled. */
    uint8_t slot_count = 0;
    for (uint8_t slot = 0; slot < SPD_MAX_SLOTS; slot++) {
        rimm_configs[slot] = 0;
        if (slot_mask & (1 << slot))
            slot_count++;
    }  

    uint8_t  slots_per_channel = slot_count / channel_count;
    uint16_t channel_size      = (mem_size >> 10) / channel_count;

    /* Populate groups. */
    spd_populate_rdram(rimm_configs, slots_per_channel, channel_size, max_module_size, rdram_device_mask);

    for (uint8_t channel_slot = 0; channel_slot < slots_per_channel; channel_slot++) {
        uint8_t  device_size  = rimm_configs[channel_slot] >> 8;
        uint8_t  device_count = rimm_configs[channel_slot] & 0xff;
        uint16_t rimm_size    = device_size * device_count;
        uint8_t  address_bits;
        uint8_t  bank_bits;

        /* Address and bank bits required for i820/i850 */
        switch(device_size) {
            /*
             * Nibbles are bit counts, and a Direct RDRAM column is a 16-byte
             * dualoct: size = 2^(rows + cols + banks) * 16 bytes. The bank
             * count sits in the low three bits of the bank byte; the upper
             * bits describe the bank architecture, and POST only accepts the
             * combinations 84h (16 banks) and c5h (32 banks).
             */
            case 32: /* 256 Mbit */
                address_bits = 0xa6; /* 1024 rows x 64 cols */
                bank_bits    = 0xc5; /* 32 banks */
                break;
            case 16: /* 128 Mbit */
                address_bits = 0x96; /* 512 rows x 64 cols */
                bank_bits    = 0xc5; /* 32 banks */
                break;
            case 8:  /* 64 Mbit */
                address_bits = 0x96; /* 512 rows x 64 cols */
                bank_bits    = 0x84; /* 16 banks */
                break;
            default:
                spd_log("SPD: Unsupported RDRAM device size %d MB on channel slot %d\n", device_size, channel_slot);
                continue;
        }
        
        uint16_t device_enable_mask = (1U << device_count) - 1;
        uint8_t  base_slot          = channel_slot * channel_count; 

        /* Allocate and populate first channel. */
        spd_modules[base_slot] = (spd_t *) calloc(1, sizeof(spd_t));
        spd_modules[base_slot]->slot = base_slot;
        spd_modules[base_slot]->size = rimm_size;
        spd_modules[base_slot]->row1 = 0;
        spd_modules[base_slot]->row2 = 0;

        rdram_data = &spd_modules[base_slot]->rdram_data;
        rdram_data->spd_revision       = 0x20;
        rdram_data->spd_size           = 0x08; /* 256 bytes */
        rdram_data->device_type        = 0x01; /* Direct RDRAM */
        rdram_data->module_type        = 0x01; /* RIMM */
        rdram_data->address_bits       = address_bits;
        rdram_data->bank_bits_byte     = bank_bits;
        rdram_data->refresh_bank_bits  = 0x04; /* 16 banks refreshed */
        /*
         * Refresh row count, in units of 32 rows, so it tracks the row-bit
         * count in address_bits. POST rejects the module (code 11) unless
         * this covers every row between refreshes.
         */
        rdram_data->refresh_interval   = 1 << ((address_bits >> 4) - 5);
        rdram_data->protocol_version   = 0x01;

        /* Standard PC800 timings */
        rdram_data->trp_min        = 0x28; /* 40 ns */
        rdram_data->tras_min       = 0x32; /* 50 ns */
        rdram_data->trcd_min       = 0x14; /* 20 ns */
        rdram_data->trr_min        = 0x14;
        rdram_data->tpp_min        = 0x14;
        /*
         * POST probes each timing range for a target value (19 ns, then 26 ns
         * on the retry) and requires min <= target <= max, giving up with POST
         * code 06 once all four ranges are rejected. Range A brackets both.
         */
        rdram_data->timing_range_a[0] = 0x13; /* min tCYCLE - 1.9 ns */
        rdram_data->timing_range_a[1] = 0x1a; /* max tCYCLE - 2.6 ns, brackets PC800 */
        rdram_data->timing_range_a[2] = 0x28; /* tRAC - 40 ns, i.e. PC800-40 */

        /*
         * 12-bit divisor POST uses as clocks = t_ns * f_MHz / fras. It is the
         * ns-to-clock scale, so 1000; a zero here is rejected with POST 14.
         */
        rdram_data->fras_high = 0x03;
        rdram_data->fras_low  = 0xe8;

        rdram_data->frequency_high = 0x03;
        rdram_data->fimax_low      = 0x20; /* 800 MHz */

        rdram_data->device_count     = device_count;
        rdram_data->device_width     = 16;
        rdram_data->device_enable[0] = device_enable_mask & 0xFF;
        rdram_data->device_enable[1] = (device_enable_mask >> 8) & 0xFF;

        rdram_data->voltage_interface = 0x01;
        rdram_data->voltage_tolerance = 0x05;

        for (int i = spd_write_part_no(rdram_data->part_number, "RDR", rimm_size);
             i < sizeof(rdram_data->part_number); i++)
            rdram_data->part_number[i] = ' ';

        rdram_data->revision_code[0]   = BCD8(EMU_VERSION_MAJ);
        rdram_data->revision_code[1]   = BCD8(EMU_VERSION_MIN);
        rdram_data->manufacturing_year = 26;
        rdram_data->manufacturing_week = 35;
        
        /* Checksums */
        for (uint8_t i = 0; i < 63; i++)
            rdram_data->checksum += spd_modules[base_slot]->data[i];
        for (uint8_t i = 99; i < 127; i++)
            rdram_data->checksum2 += spd_modules[base_slot]->data[i];

        /* Duplicate the first channel's SPD to other channels. */
        for (uint8_t channel = 1; channel < channel_count; channel++) {
            uint8_t slot = base_slot + channel;

            spd_modules[slot] = (spd_t *) calloc(1, sizeof(spd_t));
            spd_modules[slot]->slot = slot;
            spd_modules[slot]->size = rimm_size;
            spd_modules[slot]->row1 = 0;
            spd_modules[slot]->row2 = 0;

            memcpy(&spd_modules[slot]->rdram_data, &spd_modules[base_slot]->rdram_data, sizeof(spd_rdram_t));
        }
    }
    device_add(&spd_device);
}




void
spd_write_drbs(uint8_t *regs, uint8_t reg_min, uint8_t reg_max, uint8_t drb_unit)
{
    uint8_t  dimm;
    uint8_t  drb;
    uint8_t  apollo = 0;
    uint8_t  two_step = !!(drb_unit & 0x80);
    uint16_t size;
    uint16_t rows[SPD_MAX_SLOTS];

    drb_unit &= 0x7f;

    /* Special case for VIA Apollo Pro family, which jumps from 5F to 56. */
    if (reg_max < reg_min) {
        apollo  = reg_max;
        reg_max = reg_min + 7;
    }

    /* No SPD: split SIMMs into pairs as if they were "DIMM"s. */
    if (!spd_present) {
        dimm = ((reg_max - reg_min) + 1) >> 1; /* amount of "DIMM"s, also used to determine the maximum "DIMM" size */
        spd_populate(rows, dimm, mem_size >> 10, drb_unit, 1 << (log2i((machines[machine].ram.max >> 10) / dimm)), 0);
    }

    /* Write DRBs for each row. */
    spd_log("SPD: Writing DRBs... regs=[%02X:%02X] unit=%d\n", reg_min, reg_max, drb_unit);
    for (uint8_t row = 0; row <= (reg_max - reg_min); row++) {
        dimm = (row >> 1);
        size = 0;

        if (spd_present) {
            /* SPD enabled: use SPD info for this slot, if present. */
            if (spd_modules[dimm]) {
                if (spd_modules[dimm]->row1 < drb_unit) /* hack within a hack: turn a double-sided DIMM that is too small into a single-sided one */
                    size = (row & 1) ? 0 : drb_unit;
                else
                    size = (row & 1) ? spd_modules[dimm]->row2 : spd_modules[dimm]->row1;
            }
        } else {
            /* No SPD: use the values calculated above. */
            size = (rows[dimm] >> 1);
        }

        /* Determine the DRB register to write. */
        if (two_step)
            drb = reg_min + (row << 1);
        else
            drb = reg_min + row;
        if (apollo && ((drb & 0xf) < 0xa))
            drb = apollo + (drb & 0xf);

        /* Write DRB register, adding the previous DRB's value. */
        if (row == 0)
            regs[drb] = 0;
        else if (apollo && (drb == apollo))
            regs[drb] = regs[drb | 0xf]; /* 5F comes before 56 */
        else
            regs[drb] = regs[drb - 1];
        if (size)
            regs[drb] += size / drb_unit; /* this will intentionally overflow on 440GX with 2 GB */
        spd_log("SPD: DRB[%d] = %d MB (%02Xh raw)\n", row, size, regs[drb]);
    }
}

/* Needed for 430NX. */
void
spd_write_drbs_with_ext(uint8_t *regs, uint8_t reg_min, uint8_t reg_max, uint8_t drb_unit)
{
    uint8_t  dimm;
    uint8_t  drb;
    uint16_t size;
    uint16_t row_val = 0;
    uint16_t rows[SPD_MAX_SLOTS];
    int      shift;

    /* No SPD: split SIMMs into pairs as if they were "DIMM"s. */
    if (!spd_present) {
        dimm = ((reg_max - reg_min) + 1) >> 1; /* amount of "DIMM"s, also used to determine the maximum "DIMM" size */
        spd_populate(rows, dimm, mem_size >> 10, drb_unit, 1 << (log2i((machines[machine].ram.max >> 10) / dimm)), 0);
    }

    /* Write DRBs for each row. */
    spd_log("SPD: Writing DRBs... regs=[%02X:%02X] unit=%d\n", reg_min, reg_max, drb_unit);
    for (uint8_t row = 0; row <= (reg_max - reg_min); row++) {
        dimm = (row >> 1);
        size = 0;

        if (spd_present) {
            /* SPD enabled: use SPD info for this slot, if present. */
            if (spd_modules[dimm]) {
                if (spd_modules[dimm]->row1 < drb_unit) /* hack within a hack: turn a double-sided DIMM that is too small into a single-sided one */
                    size = (row & 1) ? 0 : drb_unit;
                else
                    size = (row & 1) ? spd_modules[dimm]->row2 : spd_modules[dimm]->row1;
            }
        } else {
            /* No SPD: use the values calculated above. */
            size = (rows[dimm] >> 1);
        }

        /* Determine the DRB register to write. */
        drb = reg_min + row;

        /* Write DRB register, adding the previous DRB's value. */
        if (size)
            row_val += size / drb_unit; /* this will intentionally overflow on 440GX with 2 GB */
        regs[drb] = row_val & 0xff;
        drb       = reg_min + 8 + (row >> 1);
        shift = (row & 0x01) << 2;
        /* Limit to 1 GB space, per the 430NX datasheet. */
        regs[drb] = (regs[drb] & ~(0xf << shift)) | (((row_val >> 8) & 3) << shift);
        spd_log("SPD: DRB[%d] = %d MB (%02Xh raw)\n", row, size, regs[drb]);
    }
}

/* Used by ALi M1531 and M1541/2. */
void
spd_write_drbs_interleaved(uint8_t *regs, uint8_t reg_min, uint8_t reg_max, uint8_t drb_unit)
{
    uint8_t  dimm;
    uint8_t  drb;
    uint16_t size;
    uint16_t size_acc = (drb_unit == 1) ? -1 : 0; /* M1531/M1541 = boundary - 1 */
    uint16_t rows[SPD_MAX_SLOTS];

    /* No SPD: split SIMMs into pairs as if they were "DIMM"s. */
    if (!spd_present) {
        dimm = ((reg_max - reg_min) + 1) >> 2; /* amount of "DIMM"s, also used to determine the maximum "DIMM" size */
        spd_populate(rows, dimm, mem_size >> 10, drb_unit, 1 << (log2i((machines[machine].ram.max >> 10) / dimm)), 0);
    }

    /* Write DRBs for each row. */
    spd_log("SPD: Writing DRBs... regs=[%02X:%02X] unit=%d\n", reg_min, reg_max, drb_unit);
    for (uint8_t row = 0; row <= (reg_max - reg_min); row += 2) {
        dimm = (row >> 2);
        size = 0;

        if (spd_present) {
            /* SPD enabled: use SPD info for this slot, if present. */
            if (spd_modules[dimm]) {
                if (spd_modules[dimm]->row1 < drb_unit) /* hack within a hack: turn a double-sided DIMM that is too small into a single-sided one */
                    size = ((row >> 1) & 1) ? 0 : drb_unit;
                else
                    size = ((row >> 1) & 1) ? spd_modules[dimm]->row2 : spd_modules[dimm]->row1;
            }
        } else {
            /* No SPD: use the values calculated above. */
            size = (rows[dimm] >> 1);
        }

        /* Determine the DRB register to write. */
        drb = reg_min + row;

        /* Calculate previous and new size. */
        size_acc += (size / drb_unit);

        /* Write DRB register, adding the previous DRB's value. */
        regs[drb]     = size_acc & 0xff;
        regs[drb + 1] = (regs[drb + 1] & 0xf0) | ((size_acc >> 8) & 0x0f);

        spd_log("SPD: DRB[%d] = %d MB (%02Xh raw)\n", row >> 1, size, regs[drb]);
    }
}

/* This is needed because the ALi M1621 does this stuff completely differently,
   as it has DRAM bank registers instead of DRAM row boundary registers. */
void
spd_write_drbs_ali1621(uint8_t *regs, uint8_t reg_min, uint8_t reg_max)
{
    uint8_t  dimm;
    uint8_t  drb;
    uint16_t size;
    uint16_t rows[SPD_MAX_SLOTS];

    /* No SPD: split SIMMs into pairs as if they were "DIMM"s. */
    if (!spd_present) {
        dimm = ((reg_max - reg_min) + 1) >> 2; /* amount of "DIMM"s, also used to determine the maximum "DIMM" size */
        spd_populate(rows, dimm, mem_size >> 10, 4, 1 << (log2i((machines[machine].ram.max >> 10) / dimm)), 0);
    }

    /* Write DRBs for each row. */
    spd_log("SPD: Writing DRBs... regs=[%02X:%02X] unit=%d\n", reg_min, reg_max, drb_unit);
    for (dimm = 0; dimm <= ((reg_max - reg_min) >> 2); dimm++) {
        size = 0;
        drb  = reg_min + (dimm << 2);

        regs[drb]     = 0xff;
        regs[drb + 1] = 0xff;
        regs[drb + 2] = 0x00;
        regs[drb + 3] = 0xf0;

        if (spd_modules[dimm] == NULL)
            continue;

        if (spd_present) {
            /* SPD enabled: use SPD info for this slot, if present. */
            size = (spd_modules[dimm]->row1 + spd_modules[dimm]->row2) >> 1;
        } else {
            /* No SPD: use the values calculated above. */
            size = (rows[dimm] >> 1);
        }

        if (spd_modules[dimm]->row1)
            regs[drb + 3] |= 0x06;

        switch (size) {
            default:
            case 4:
                regs[drb + 2] = 0x00;
                break;
            case 8:
                regs[drb + 2] = 0x10;
                break;
            case 16:
                regs[drb + 2] = 0x20;
                break;
            case 32:
                regs[drb + 2] = 0x30;
                break;
            case 64:
                regs[drb + 2] = 0x40;
                break;
            case 128:
                regs[drb + 2] = 0x50;
                break;
            case 256:
                regs[drb + 2] = 0x60;
                break;
        }

        if (spd_modules[dimm]->row2) {
            regs[drb + 3] |= 0x01;
            regs[drb + 2] |= 0x80;
        }

        spd_log("SPD: DIMM %i: %02X %02X %02X %02X\n", regs[drb], regs[drb + 1], regs[drb + 2], regs[drb + 3]);
    }
}

/* This is needed because the AMD 751 does this stuff completely differently,
   as it has DRAM bank registers instead of DRAM row boundary registers. */
void
spd_write_drbs_amd751(uint8_t *regs, uint8_t reg_min, uint8_t reg_max, uint8_t map_min, uint8_t map_max)
{
    uint8_t  dimm;
    uint8_t  drb;
    uint8_t  map;
    uint16_t size;
    uint16_t bank_addr = 0;

    /* Write DRBs for each row. */
    spd_log("SPD: Writing DRBs... regs=[%02X:%02X]\n", reg_min, reg_max);
    for (dimm = 0; dimm <= ((reg_max - reg_min) >> 1); dimm++) {
        size = 0;
        drb  = reg_min + (dimm << 1);
        map = map_min + (dimm >> 1);

        if (spd_modules[dimm] == NULL)
            continue;

        /* SPD enabled: use SPD info for this slot, if present. */
        size = (spd_modules[dimm]->row1 + spd_modules[dimm]->row2) >> 1;
        bank_addr += size >> 2;

        regs[drb]     = (bank_addr & 1) << 7;
        regs[drb + 1] = (bank_addr >> 1);

        if (spd_modules[dimm]->row1 || spd_modules[dimm]->row2)
            regs[drb] |= 0x01;

        if (spd_modules[dimm]->row2)
            regs[map] |= 0x02 << ((dimm & 1) << 2);

        switch (size) {
            default:
            case 4:
                regs[drb] |= 0x00;
                break;
            case 8:
                regs[drb] |= 0x02;
                break;
            case 16:
                regs[drb] |= 0x06;
                break;
            case 32:
                regs[drb] |= 0x0e;
                break;
            case 64:
                regs[drb] |= 0x1e;
                break;
            case 128:
                regs[drb] |= 0x3e;
                break;
            case 256:
                regs[drb] |= 0x7e;
                break;
        }

        spd_log("SPD: DIMM %i: %02X %02X %02X\n", dimm, regs[drb], regs[drb + 1], regs[map]);
    }
}

void
spd_write_drbs_intel_815ep(uint8_t *regs)
{
    /* All Intel MCH based boards demand SPD so we ignore completely the non-SPD calculations */
    int      size;
    int      reg_apply;
    uint16_t rows[SPD_MAX_SLOTS];

    if (!spd_present)
        spd_populate(rows, 3, mem_size << 10, 32, 512, 0);

    /* Clear previous configurations */
    regs[0x52] = regs[0x54] = 0;

    /* Write DRBs for each row. */
    for (int slot = 0; slot < 3; slot++) {
        if(spd_modules[slot] == NULL)
            break;
        size = spd_modules[slot]->row1 + spd_modules[slot]->row2;
        spd_log("Intel 815EP SPD: Registering Slot %d with size %dMB.\n", slot, size);

        /* Calculate Size. Nullify if the size is illegal. */
        switch (size) {
            default:
                reg_apply = 0;
                spd_log("Intel 815EP SPD: Illegal Size on Slot %d. Size not divisible by 32.\n", slot);
                break;

            case 32:
                reg_apply = 1;
                break;

            case 48:
                reg_apply = 3;
                break;

            case 64:
                reg_apply = 4;
                break;

            case 96:
                reg_apply = 6;
                break;

            case 128:
                reg_apply = 7;
                break;

            case 192:
                reg_apply = 11;
                break;

            case 256:
                reg_apply = 12;
                break;

            case 512:
                reg_apply = 15;
                break;
        }

        /* Write on the representative register */
        switch (slot) {
            case 0:
                regs[0x52] |= reg_apply;
                break;

            case 1:
                regs[0x52] |= reg_apply << 4;
                break;

            case 2:
                regs[0x54] |= reg_apply;
                break;

            default:
                break;
        }
    }
}

void
spd_write_drbs_intel_845(uint8_t *regs)
{
    spd_write_drbs(regs, 0x60, 0x65, 32);

    regs[0x66] = regs[0x65];
    regs[0x67] = regs[0x65];
}

uint16_t
spd_write_gar_gbar_intel_850(uint8_t *regs)
{
    uint8_t  group = 0;
    uint16_t cumulative_mb = 0;

    /* Initialise GARs to 0x80 */
    memset(&regs[0x40], 0x80, 16);

    /* Initialise GBARs to 0x0001 */
    for (int gbar = 0x60; gbar <= 0x7e; gbar += 2) {
        regs[gbar] = 0x01;
        regs[gbar + 1] = 0x00;
    }

    if (!spd_present) {
        /* Fallback: Get total RAM in MB from mem_size */
        uint16_t remaining = mem_size >> 10;

        /* Break total RAM into 256MB/128MB group chunks */
        while ((remaining > 0) && (group < 8)) {
            uint16_t chunk = MIN(remaining, 256);
            cumulative_mb += chunk;

            /* Manually program GAR */
            regs[0x40 + group] = (chunk >= 64) ? 0x84 : 0x82;

            /* Manually program GBAR */
            uint16_t gbar_val = ((cumulative_mb >> 4) & 0x07ff) | ((group & 0x07) << 11);
            regs[0x60 + (group << 1)]     = gbar_val & 0xff;
            regs[0x60 + (group << 1) + 1] = (gbar_val >> 8) & 0x3f;

            remaining -= chunk;
            group++;
        }
    }

    /* Iterate over RIMM slot pairs (0,1), (2,3) */
    for (uint8_t slot = 0; (slot < SPD_MAX_SLOTS) && (group < 16); slot += 2) {
        if (!spd_modules[slot] || !spd_modules[slot + 1])
            continue; /* Unpopulated pair / CRIMM */

        spd_rdram_t *rdram = &spd_modules[slot]->rdram_data;
        uint8_t      device_count = rdram->device_count;
        uint16_t     pair_size_mb;
        uint8_t      gar_val;

        /* Determine technology and device-pair size */
        if (rdram->address_bits == 0xd9 || (rdram->bank_bits_byte >= 0x05)) {
            pair_size_mb = 64;   /* 256-Mb: 32 MB on ChA + 32 MB on ChB */
            gar_val      = 0x84; /* 1 KB page (0x84) or 2 KB page (0xD4), DDT = 10b */
        } else {
            pair_size_mb = 32;   /* 128-Mb: 16 MB on ChA + 16 MB on ChB */
            gar_val      = 0x82; /* 1 KB page, DDT = 01b */
        }

        uint8_t device_pairs = device_count;

        /* Each active group can hold up to 4 device-pairs */
        while ((device_pairs > 0) && (group < 8)) {
            uint8_t  group_pairs = MIN(device_pairs, 4);
            uint16_t group_size  = group_pairs * pair_size_mb;
            cumulative_mb += group_size;

            /* Set GAR: technology code with Bit 7 (disabled) = 0 */
            regs[0x40 + group] = gar_val;

            /* Set GBAR: boundary in 16MB units + GID in bits [13:11] */
            uint16_t gbar_val = (cumulative_mb >> 4) & 0x07ff;
            gbar_val |= ((group & 0x07) << 11);

            regs[0x60 + (group << 1)]     = gbar_val & 0xff;
            regs[0x60 + (group << 1) + 1] = (gbar_val >> 8) & 0x3f;

            device_pairs -= group_pairs;
            group++;
        }
    }

    /* Fill all trailing unpopulated GBARs up to GBAR15 with Top-of-Memory */
    uint16_t last_boundary = (cumulative_mb >> 4) & 0x07ff;
    for (uint8_t g = group; g < 16; g++) {
        uint16_t gbar_val = last_boundary | ((g & 0x07) << 11);
        regs[0x60 + (g << 1)]     = gbar_val & 0xff;
        regs[0x60 + (g << 1) + 1] = (gbar_val >> 8) & 0x3f;
    }

    return cumulative_mb;
}

static const device_t spd_device = {
    .name          = "Serial Presence Detect ROMs",
    .internal_name = "spd",
    .flags         = DEVICE_ISA,
    .local         = 0,
    .init          = spd_init,
    .close         = spd_close,
    .reset         = NULL,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};
