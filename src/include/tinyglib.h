/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Minimal reimplementation of GLib for libslirp.
 *
 *
 *
 * Authors: RichardG, <richardg867@gmail.com>
 *
 *          Copyright 2020 RichardG.
 */
#ifndef TINYGLIB_H
# define TINYGLIB_H

#include <glib.h>
/* Types */

/* Windows does not define ssize_t, so we need to define it here. */
#ifndef __APPLE__
#ifndef _SSIZE_T_DEFINED
# define _SSIZE_T_DEFINED
# undef ssize_t
# ifdef _WIN64
#  define ssize_t int64_t
# else
#  define ssize_t int32_t
# endif
#endif
#endif

#endif
