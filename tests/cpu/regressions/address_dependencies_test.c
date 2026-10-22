/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdint.h>
#include <stdio.h>
#include "cpu.h"
#include "codegen_timing_common.h"

int main(void)
{
    unsigned checked = 0;
    /* Intel SDM Vol. 2, Table 2-3: all base/index/scale and mod combinations. */
    for (unsigned mod = 0; mod < 4; ++mod) {
        for (unsigned sib = 0; sib < 256; ++sib) {
            unsigned base = sib & 7, index = (sib >> 3) & 7;
            uint32_t expected = 0;
            if (mod != 3) {
                if (mod || base != 5) expected |= 1u << base;
                if (index != 4) expected |= 1u << index;
            }
            uint32_t actual = get_addr_regmask(MODRM, (mod << 6) | 4 | (sib << 8), 0x300);
            if (actual != expected) {
                fprintf(stderr, "mod=%u sib=%02x: %02x != %02x\n", mod, sib, actual, expected);
                return 1;
            }
            ++checked;
        }
    }
    printf("%u SIB dependency masks passed\n", checked);
    return 0;
}
