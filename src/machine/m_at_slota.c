/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Implementation of Slot A machines.
 *
 *
 *
 * Authors: Melody Goad, <mszoopers@protonmail.com>
 *
 *          Copyright 2023 Melody Goad.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <86box/86box.h>
#include <86box/mem.h>
#include <86box/io.h>
#include <86box/rom.h>
#include <86box/pci.h>
#include <86box/device.h>
#include <86box/chipset.h>
#include <86box/hdc.h>
#include <86box/hdc_ide.h>
#include <86box/keyboard.h>
#include <86box/flash.h>
#include <86box/sio.h>
#include <86box/hwm.h>
#include <86box/spd.h>
#include <86box/video.h>
#include "cpu.h"
#include <86box/machine.h>
#include <86box/clock.h>
#include <86box/sound.h>
#include <86box/snd_ac97.h>

/*
 * ASUS K7M
 *
 * North Bridge: AMD 751
 * South Bridge: VIA VT82C686A
 * Additional Devices: Analog Devices AD1881
 */
int
machine_at_k7m_init(const machine_t *model)
{
    int ret;

    ret = bios_load_linear("roms/machines/k7m/bios.rom",
                           0x000c0000, 262144, 0);

    if (bios_only || !ret)
        return ret;

    machine_at_common_init(model);

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_slot(0x00, PCI_CARD_NORTHBRIDGE, 0, 0, 0, 0);
    pci_register_slot(0x01, PCI_CARD_AGPBRIDGE,   1, 2, 3, 4);
    pci_register_slot(0x1e, PCI_CARD_BRIDGE,      0, 0, 0, 0);
    pci_register_slot(0x04, PCI_CARD_SOUTHBRIDGE, 1, 2, 8, 4);
    pci_register_slot(0x02, PCI_CARD_AGP,         1, 2, 3, 4);
    pci_register_slot(0x03, PCI_CARD_NORMAL,      1, 2, 3, 4);
    pci_register_slot(0x10, PCI_CARD_NORMAL,      2, 3, 4, 1);
    pci_register_slot(0x05, PCI_CARD_NORMAL,      3, 4, 1, 2);
    pci_register_slot(0x06, PCI_CARD_NORMAL,      4, 1, 2, 3);
    pci_register_slot(0x07, PCI_CARD_NORMAL,      1, 2, 3, 4);

    device_add(&amd751_device);             /* AMD 751 */
    device_add(&via_vt82c686a_device);      /* VIA VT82C686A */
    device_add(&sst_flash_39sf020_device);  /* SST 4Mbit Flash */
    spd_register(SPD_TYPE_SDRAM, 0x7, 512); /* SPD */

    return ret;
}

/*
 * EPoX EP-7KXA
 *
 * North Bridge: VIA VT8371 (Apollo KX133)
 * South Bridge: VIA VT82C686A
 */
int
machine_at_7kxa_init(const machine_t *model)
{
    int ret;

    ret = bios_load_linear("roms/machines/7kxa/k7001809.bin",
                           0x000c0000, 262144, 0);

    if (bios_only || !ret)
        return ret;

    machine_at_common_init(model);

    pci_init(PCI_CONFIG_TYPE_1);
    /* UNVERIFIED: PCI slot IRQ routing not confirmed against the EP-7KXA; the south
       bridge must be at device 7, which is where the BIOS probes for it. */
    pci_register_slot(0x00, PCI_CARD_NORTHBRIDGE, 0, 0, 0, 0);
    pci_register_slot(0x01, PCI_CARD_AGPBRIDGE,   1, 2, 3, 4);
    pci_register_slot(0x07, PCI_CARD_SOUTHBRIDGE, 1, 2, 3, 4);
    pci_register_slot(0x08, PCI_CARD_NORMAL,      1, 2, 3, 4);
    pci_register_slot(0x09, PCI_CARD_NORMAL,      2, 3, 4, 1);
    pci_register_slot(0x0A, PCI_CARD_NORMAL,      3, 4, 1, 2);
    pci_register_slot(0x0B, PCI_CARD_NORMAL,      4, 1, 2, 3);
    pci_register_slot(0x0C, PCI_CARD_NORMAL,      1, 2, 3, 4);

    device_add(&via_vt8371_device);             /* VIA Apollo KX133 */
    device_add(&via_vt82c686a_device);          /* VIA VT82C686A */
    device_add(&sst_flash_39sf020_device);      /* SST 4Mbit Flash */
    device_add(ics9xxx_get(CY_W210));           /* Cypress/IC Works W210 Clock Chip */
    spd_register(SPD_TYPE_SDRAM, 0x7, 512);     /* UNVERIFIED: DIMM slot count not confirmed against the board; KX133 silicon supports 4. */

    return ret;
}
