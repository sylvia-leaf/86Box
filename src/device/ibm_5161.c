/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          Emulation of the IBM Expansion Unit (5161), and of generic
 *          ISA expansion chassis: a card in a slot of the system unit,
 *          cabled to a unit with slots of its own.
 *
 * Authors: Miran Grca, <mgrca8@gmail.com>
 *
 *          Copyright 2016-2018 Miran Grca.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <86box/86box.h>
#include <86box/device.h>
#include <86box/io.h>
#include <86box/apm.h>
#include <86box/dma.h>
#include <86box/mem.h>
#include <86box/pci.h>
#include <86box/timer.h>
#include <86box/pit.h>
#include <86box/plat_unused.h>
#include <86box/port_92.h>
#include <86box/machine.h>
#include <86box/ini.h>
#include <86box/config.h>
#include <86box/ibm_5161.h>

typedef struct ibm_5161_t {
    uint8_t regs[8];
} ibm_5161_t;

static void
ibm_5161_out(uint16_t port, uint8_t val, void *priv)
{
    ibm_5161_t *dev = (ibm_5161_t *) priv;

    dev->regs[port & 0x0007] = val;
}

static uint8_t
ibm_5161_in(uint16_t port, void *priv)
{
    const ibm_5161_t *dev = (ibm_5161_t *) priv;
    uint8_t           ret = 0xff;

    ret = dev->regs[port & 0x0007];

    switch (port) {
        case 0x210: /* Write to latch expansion bus data (ED0-ED7) */
                    /* Read to verify expansion bus data (ED0-ED7) */
            break;
        case 0x214: /* Write to latch data bus bits (DO - 07) */
                    /* Read data bus bits (DO - D7) */
            break;
        case 0x211: /* Read high-order address bits (A8 - A 15) */
        case 0x215: /* Read high-order address bits (A8 - A 15) */
            ret = (get_last_addr() >> 8) & 0xff;
            break;
        case 0x212: /* Read low-order address bits (A0 - A7) */
        case 0x216: /* Read low-order address bits (A0 - A7) */
            ret = get_last_addr() & 0xff;
            break;
        case 0x213: /* Write 00 to disable expansion unit */
                    /* Write 01 to enable expansion unit */
                    /* Read status of expansion unit
                                    00 = enable/disable
                                    01 = wait-state request flag
                                    02-03 = not used
                                    04-07 = switch position
                                            1 = Off
                                            0 = On */
            ret = (dev->regs[3] & 0x01) | (((~(0xf - ((mem_size + isa_mem_size) >> 6))) & 0xf) << 4);
            break;

        default:
            break;
    }

    return ret;
}

static void
ibm_5161_close(void *priv)
{
    ibm_5161_t *dev = (ibm_5161_t *) priv;

    free(dev);
}

static void *
ibm_5161_init(UNUSED(const device_t *info))
{
    ibm_5161_t *dev = (ibm_5161_t *) calloc(1, sizeof(ibm_5161_t));

    /* Extender Card Registers */
    io_sethandler(0x0210, 0x0004,
                  ibm_5161_in, NULL, NULL, ibm_5161_out, NULL, NULL, dev);

    /* Receiver Card Registers */
    io_sethandler(0x0214, 0x0003,
                  ibm_5161_in, NULL, NULL, ibm_5161_out, NULL, NULL, dev);

    return dev;
}

const device_t ibm_5161_device = {
    .name          = "IBM Expansion Unit (5161)",
    .internal_name = "ibm_5161",
    .flags         = DEVICE_ISA,
    .local         = 0,
    .init          = ibm_5161_init,
    .close         = ibm_5161_close,
    .reset         = NULL,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};

/* A generic ISA expansion chassis: an extender card and a backplane with
   as many slots as configured, 8-bit or 16-bit. Nothing of it is seen by
   software; it gives the cards more slots. */
static void *
isa_chassis_init(UNUSED(const device_t *info))
{
    return calloc(1, 1);
}

static void
isa_chassis_close(void *priv)
{
    free(priv);
}

static const device_config_t isa_chassis_config[] = {
    // clang-format off
    {
        .name           = "slots",
        .description    = "Slots",
        .type           = CONFIG_SELECTION,
        .default_string = NULL,
        .default_int    = 8,
        .file_filter    = NULL,
        .spinner        = { 0 },
        .selection      = {
            { .description = "4",  .value = 4  },
            { .description = "6",  .value = 6  },
            { .description = "8",  .value = 8  },
            { .description = "10", .value = 10 },
            { .description = "12", .value = 12 },
            { .description = ""                }
        },
        .bios           = { { 0 } }
    },
    { .name = "", .description = "", .type = CONFIG_END }
    // clang-format on
};

const device_t isa_chassis_8_device = {
    .name          = "ISA Expansion Chassis (8-bit)",
    .internal_name = "isa_chassis_8",
    .flags         = DEVICE_ISA,
    .local         = 8,
    .init          = isa_chassis_init,
    .close         = isa_chassis_close,
    .reset         = NULL,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = isa_chassis_config
};

const device_t isa_chassis_16_device = {
    .name          = "ISA Expansion Chassis (16-bit)",
    .internal_name = "isa_chassis_16",
    .flags         = DEVICE_ISA16,
    .local         = 16,
    .init          = isa_chassis_init,
    .close         = isa_chassis_close,
    .reset         = NULL,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = isa_chassis_config
};

/* The units chosen as isa_expansion_type. The IBM 5161 is not one of
   them: IBM's machines that take it have it as an option of their own
   (enable_5161). */
static const struct {
    const device_t *dev;
} units[] = {
    // clang-format off
    { &device_none           },
    { &isa_chassis_8_device  },
    { &isa_chassis_16_device },
    { NULL                   }
    // clang-format on
};

void
isa_expansion_reset(void)
{
    if ((isa_expansion_type == 0) || !isa_expansion_available(isa_expansion_type, machine))
        return;

    device_add(units[isa_expansion_type].dev);
}

const device_t *
isa_expansion_get_device(int unit)
{
    return units[unit].dev;
}

const char *
isa_expansion_get_internal_name(int unit)
{
    return device_get_internal_name(units[unit].dev);
}

int
isa_expansion_get_from_internal_name(const char *str)
{
    for (int c = 0; units[c].dev != NULL; c++) {
        if (!strcmp(units[c].dev->internal_name, str))
            return c;
    }

    return 0;
}

int
isa_expansion_has_config(int unit)
{
    return (units[unit].dev != NULL) && (units[unit].dev->config != NULL);
}

/* A chassis goes with any machine that has ISA slots; a 16-bit one with
   one that has 16-bit slots. */
int
isa_expansion_available(int unit, int machine_id)
{
    if (units[unit].dev == &isa_chassis_16_device)
        return machine_has_bus(machine_id, MACHINE_BUS_ISA16);

    return (unit == 0) || machine_has_bus(machine_id, MACHINE_BUS_ISA);
}

/* The slots a unit has for cards: the 5161's eight but the one its
   receiver card takes, a chassis's as configured. */
int
isa_expansion_slots(const device_t *dev)
{
    device_context_t context;

    if (dev == &ibm_5161_device)
        return 7;
    if ((dev != &isa_chassis_8_device) && (dev != &isa_chassis_16_device))
        return 0;

    device_set_context(&context, dev, 0);
    return config_get_int(context.name, "slots", 8);
}

/* What the unit's slots are: 8-bit ISA, or 16-bit. */
int
isa_expansion_slot_kind(const device_t *dev)
{
    return (dev == &isa_chassis_16_device) ? SLOT_ISA16 : SLOT_ISA8;
}
