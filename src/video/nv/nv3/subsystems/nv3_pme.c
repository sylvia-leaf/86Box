/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          NV3 pme: Nvidia Mediaport - External MPEG Decode Interface
 *
 *
 *
 * Authors: Connor Hyde, <mario64crashed@gmail.com> I need a better email address ;^)
 *
 *          Copyright 2024-2025 starfrost
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <86box/86box.h>
#include <86box/device.h>
#include <86box/mem.h>
#include <86box/pci.h>
#include <86box/rom.h> // DEPENDENT!!!
#include <86box/video.h>
#include <86box/nv/vid_nv.h>
#include <86box/nv/vid_nv3.h>

nv_register_t pme_registers[] = {
    { NV3_PME_INTR, "PME - Interrupt Status", NULL, NULL},
    { NV3_PME_INTR_EN, "PME - Interrupt Enable", NULL, NULL,},
    { NV_REG_LIST_END, NULL, NULL, NULL}, // sentinel value 
};

void nv3_pme_init(void)
{  
    nv_log("Initialising PME...");

    nv_log("Done\n");
}

uint32_t nv3_pme_read(uint32_t address) 
{ 
    nv_register_t* reg = nv_get_register(address, pme_registers, sizeof(pme_registers)/sizeof(pme_registers[0]));

    uint32_t ret = 0x00;

    // todo: friendly logging

    nv_log_verbose_only("PME Read from 0x%08x", address);

    // if the register actually exists
    if (reg)
    {
        // on-read function
        if (reg->on_read)
            ret = reg->on_read();
        else
        {   
            // Interrupt state:
            // Bit 0 - Image Notifier
            // Bit 4 - Vertical Blank Interval Notifier
            // Bit 8 - Video Notifier
            // Bit 12 - Audio Notifier
            // Bit 16 - VMI Notifer
            switch (reg->address)
            {
                case NV3_PME_INTR:
                    ret = nv3->pme.interrupt_status;
                    break;
                case NV3_PME_INTR_EN:
                    ret = nv3->pme.interrupt_enable;
                    break;
            }
        }

        if (reg->friendly_name)
            nv_log_verbose_only(": 0x%08x <- %s\n", ret, reg->friendly_name);
        else   
            nv_log_verbose_only("\n");
    }
    else
    {
        nv_log(": Unknown register read (address=0x%08x), returning 0x00\n", address);
    }

    return ret;
}

void nv3_pme_write(uint32_t address, uint32_t value) 
{
    nv_register_t* reg = nv_get_register(address, pme_registers, sizeof(pme_registers)/sizeof(pme_registers[0]));

    nv_log_verbose_only("PME Write 0x%08x -> 0x%08x\n", value, address);

    // if the register actually exists
    if (reg)
    {
        if (reg->friendly_name)
            nv_log_verbose_only(": %s\n", reg->friendly_name);
        else   
            nv_log_verbose_only("\n");

        // on-read function
        if (reg->on_write)
            reg->on_write(value);   
        else
        {
            switch (reg->address)
            {
                // Interrupt state:
                // Bit 0 - Image Notifier
                // Bit 4 - Vertical Blank Interfal Notifier
                // Bit 8 - Video Notifier
                // Bit 12 - Audio Notifier
                // Bit 16 - VMI Notifer

                case NV3_PME_INTR:
                    nv3->pme.interrupt_status &= ~value;
                    nv3_pmc_clear_interrupts();
                    break;
                case NV3_PME_INTR_EN:
                    nv3->pme.interrupt_enable = value & 0x00001111;
                    break;
            }
        }

    }
    else /* Completely unknown */
    {
        nv_log(": Unknown register write (address=0x%08x)\n", address);
    }

}