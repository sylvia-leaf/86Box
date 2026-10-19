/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Debug console: registers and memory of the paused machine.
 *
 * Authors: Michael Pratte, <mpratte@makefox.group>
 *
 *          Copyright 2026 Michael Pratte.
 */
#ifndef EMU_DEBUGCON_H
#define EMU_DEBUGCON_H

#ifdef __cplusplus
extern "C" {
#endif

/* The consoles' end: where results go (called on the emulation thread,
   with the console the command came from: 0 the window, others a network
   client), and text to run, a command a line. Submitting answers 0 when the
   queue is full. */
extern void debugcon_init(void (*output)(int source, const char *text));
extern int  debugcon_submit(int source, const char *text);

/* Commands that read nothing of the machine's, answered at once. */
extern const char *debugcon_help_text(void);

/* The emulation thread's end, from its loop while paused. */
extern void debugcon_poll(void);

#ifdef __cplusplus
}
#endif

#endif /*EMU_DEBUGCON_H*/
