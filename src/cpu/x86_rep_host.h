/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          The host's side of the REP string instructions: whole runs of
 *          MOVS, STOS, SCAS and CMPS elements done on host memory at once,
 *          once x86_rep_fast.h has shown the run is plain RAM on both sides.
 *          Nothing here touches processor state.
 *
 * Authors: Michael Pratte, <mpratte@makefox.group>
 *
 *          Copyright 2026 Michael Pratte.
 */
#ifndef EMU_X86_REP_HOST_H
#define EMU_X86_REP_HOST_H

#include <stdint.h>

/* How REP string runs are done, set once by rep_host_detect(). */
enum {
    REP_HOST_OFF     = 0, /* every element through the per-element path */
    REP_HOST_GENERIC = 1  /* whole runs, baseline host instructions */
};
extern uint8_t rep_host_mode;

/* At startup: the kernels this host runs. */
extern void rep_host_detect(void);

/* The bytes [dlo, dlo + bytes) take [slo, slo + bytes), copied in the
   guest's order: upward, or downward when down is set. The caller has
   checked that this equals doing it element by element: the ranges do
   not overlap, or the destination trails the source in that order. */
extern void rep_host_copy(uint8_t *dlo, const uint8_t *slo, uint32_t bytes, int down);

/* The same, element by element, for a destination that leads its source
   (the pattern-replicating overlap). d0 and s0 are element 0. */
extern void rep_host_copy_elems(uint8_t *d0, const uint8_t *s0, uint32_t n, uint32_t esz, int down);

/* n elements of esz bytes, [dlo, dlo + n * esz), set to value, stored in
   the guest's order. */
extern void rep_host_fill(uint8_t *dlo, uint32_t value, uint32_t n, uint32_t esz, int down);

/* Of the n elements at [lo, lo + n * esz), taken in the guest's order
   (from the top when down is set), how many in a row compare equal to
   value (repe) or not equal to it (!repe): the elements a REPE or REPNE
   SCAS goes on past. n when none stops it. */
extern uint32_t rep_host_scan(const uint8_t *lo, uint32_t n, uint32_t esz, int down, uint32_t value, int repe);

/* The same for two runs compared element by element (CMPS). */
extern uint32_t rep_host_cmp(const uint8_t *alo, const uint8_t *blo, uint32_t n, uint32_t esz, int down, int repe);

#endif /*EMU_X86_REP_HOST_H*/
