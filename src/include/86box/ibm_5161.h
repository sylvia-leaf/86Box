/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Emulation of the IBM Expansion Unit (5161).
 *
 * Authors: Miran Grca, <mgrca8@gmail.com>
 *
 *          Copyright 2016-2018 Miran Grca.
 */
#ifndef EMU_IBM_5161_H
#define EMU_IBM_5161_H

extern const device_t ibm_5161_device;
extern const device_t isa_chassis_8_device;
extern const device_t isa_chassis_16_device;

/* The ISA expansion units: an extender card in a slot of the system unit,
   cabled to a receiver card in a unit with slots of its own. A unit adds
   slots and nothing else; the cards in it are on the one ISA bus. */
extern void            isa_expansion_reset(void);
extern const device_t *isa_expansion_get_device(int unit);
extern const char     *isa_expansion_get_internal_name(int unit);
extern int             isa_expansion_get_from_internal_name(const char *str);
extern int             isa_expansion_has_config(int unit);
extern int             isa_expansion_available(int unit, int machine_id);
extern int             isa_expansion_slots(const device_t *dev);
extern int             isa_expansion_slot_kind(const device_t *dev);

#endif /*EMU_IBM_5161_H*/
