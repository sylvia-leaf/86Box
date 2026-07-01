/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          ATi Mach64 graphics card 2D-accelerated portion.
 *          The ATi Mach64 is a 1994 Windows 2D accelerator.
 *          Technical information is available at: https://bitsavers.org/components/ati/RRG-S00700-05_atirage_Register_Reference_Guide_1999410.pdf
 *
 * Authors: Sarah Walker, <https://pcem-emulator.co.uk/>
 *          Miran Grca, <mgrca8@gmail.com>
 *          Connor Hyde, <mario64crashed@gmail.com> <https://starfrost.net>
 *
 *          Copyright 2008-2019 Sarah Walker.
 *          Copyright 2016-2019 Miran Grca.
 *          Copyright 2026 Connor Hyde.
 */

#include "vid_ati_rage.h"

static void
atirage_accel_write_fifo_l(atirage_t *atirage, uint32_t addr, uint32_t val);

static int
atirage_write_trap(atirage_t *atirage, uint32_t *cpu_dat, int *count);

static void
atirage_recalc_dp_set_engine(atirage_t *atirage)
{
    static const unsigned int pitches[16] =
    {
        [0] = 320, // fallback
        [1] = 320,
        [2] = 352,
        [3] = 384,
        [4] = 640,
        [5] = 800,
        [6] = 896,
        [7] = 512,
        [8] = 1024,
        [9] = 1152,
        [10] = 1280,
        [11] = 400,
        [12] = 832,
        [13] = 1600,
        [14] = 448,
        [15] = 2048
    };

    atirage->dst_y_x = 0;
    atirage->dst_height_width = 0;
    atirage->src_y_x = 0;
    atirage->sc_top_bottom = 0x3FFF0000;
    atirage->sc_left_right = 0x1FFF0000;
    atirage->write_mask = ~0u;
    atirage->clr_cmp_clr = 0;
    atirage->src_y_x_start = 0;
    atirage->src_cntl &= ~((3 << 13) | (1 << 5) | (1 << 12));
    atirage->dst_cntl &= ~(7 << 13);
    atirage->dp_pix_width &= (1 << 13);

    atirage->dp_pix_width = (atirage->dp_pix_width & ~7) | ((atirage->dp_set_gui_engine >> 3) & 7);
    atirage->dp_pix_width = (atirage->dp_pix_width & ~(0xf << 8)) | ((atirage->dp_set_gui_engine & (1 << 6)) ? ((atirage->dp_pix_width & 7) << 8) : 0);

    atirage->dst_off_pitch = (262144 * ((atirage->dp_set_gui_engine >> 7) & 3)) & ((1 << 20) - 1);
    atirage->dst_off_pitch |= (((pitches[(atirage->dp_set_gui_engine >> 10) & 0xF]) * ((atirage->dp_set_gui_engine & (1 << 14)) ? 2 : 1)) / 8) << 22;

    atirage->src_off_pitch = 0;
    if (atirage->dp_set_gui_engine & (1 << 15))
        atirage->src_off_pitch = atirage->dst_off_pitch;

    switch ((atirage->dp_set_gui_engine >> 16) & 3)
    {
        case 0:
            atirage->src_height1_width1 = atirage->src_height2_width2 = (0x00080008);
            break;
        case 1:
            atirage->src_height1_width1 = atirage->src_height2_width2 = (0x00200001);
            break;
        case 2:
            atirage->src_height1_width1 = atirage->src_height2_width2 = (0x00180008);
            break;
    }

    switch ((atirage->dp_set_gui_engine >> 20) & 0xf)
    {
        case 0:
            pclog("unknown drawing combo2\n");
            break;
        case 1:
            atirage->dp_mix = 0x070003;
            atirage->dp_src = 0x0000100;
            atirage->gui_traj_cntl = 0x23;
            break;
        case 2:
            atirage->dp_src = 0x200;
            atirage->dp_mix = 0x70007;
            atirage->gui_traj_cntl = 0x3;
            break;
        case 3:
            atirage->dp_src = 0x20100;
            atirage->dp_mix = 0x70007;
            atirage->gui_traj_cntl = 0x3;
            break;
        case 4:
            atirage->dp_src = 0x100;
            atirage->dp_mix = 0x70007;
            atirage->gui_traj_cntl = 0x3;
            break;
        case 5:
            atirage->dp_src = 0x10100;
            atirage->dp_mix = 0x70007;
            atirage->gui_traj_cntl = 0x01000003;
            break;
        case 6:
            atirage->dp_src = 0x100;
            atirage->dp_mix = 0x70007;
            atirage->gui_traj_cntl = 0x3;
            break;
        case 7:
            atirage->dp_src = 0x300;
            atirage->dp_mix = 0x70007;
            atirage->gui_traj_cntl = 0x30003;
            break;
        case 8:
            atirage->dp_src = 0x300;
            atirage->dp_mix = 0x70007;
            atirage->gui_traj_cntl = 0;
            break;
        case 9:
            atirage->dp_src = 0x300;
            atirage->dp_mix = 0x70007;
            atirage->gui_traj_cntl = 1;
            break;
        case 10:
            atirage->dp_src = 0x300;
            atirage->dp_mix = 0x70007;
            atirage->gui_traj_cntl = 2;
            break;
        case 11:
            atirage->dp_src = 0x300;
            atirage->dp_mix = 0x70007;
            atirage->gui_traj_cntl = 3;
            break;
        case 12:
            atirage->dp_src = 0x20100;
            atirage->dp_mix = 0x70003;
            atirage->gui_traj_cntl = 0x1004001B;
            break;
        case 13:
            atirage->dp_src = 0x20100;
            atirage->dp_mix = 0x70003;
            atirage->gui_traj_cntl = 0x0004001B;
            break;
        case 14:
            pclog("unknown drawing combo\n");
            break;
        case 15:
            atirage->dp_src = 0x300;
            atirage->dp_mix = 0x70007;
            atirage->gui_traj_cntl = 0x0004001B;
            break;
    }
    atirage_accel_write_fifo_l(atirage, 0x330, atirage->gui_traj_cntl);
}

static void
atirage_accel_write_fifo(atirage_t *atirage, uint32_t addr, uint8_t val)
{
    switch (addr & 0x3ff) {
        case 0x100 ... 0x103:
            WRITE8(addr, atirage->dst_off_pitch, val);
            break;
        case 0x104 ... 0x105:
        case 0x11c ... 0x11d:
            WRITE8(addr + 2, atirage->dst_y_x, val);
            break;
        case 0x108 ... 0x109:
            WRITE8(addr, atirage->dst_y_x, val);
            break;
        case 0x10c ... 0x10f: /* 0_43h DST_Y_X */
        case 0x134 ... 0x137: /* 0_4Dh DST_Y_X Rage alias */
            WRITE8(addr, atirage->dst_y_x, val);
            break;
        case 0x2e8 ... 0x2eb:
            WRITE8(addr ^ 2, atirage->dst_y_x, val);
            break;
        case 0x110 ... 0x111:
            WRITE8(addr + 2, atirage->dst_height_width, val);
            break;
        case 0x114 ... 0x115:
        case 0x118 ... 0x11b:
        case 0x11e ... 0x11f:
            WRITE8(addr, atirage->dst_height_width, val);
            fallthrough;
        case 0x113:
            if (((addr & 0x3ff) == 0x11b || (addr & 0x3ff) == 0x11f || (addr & 0x3ff) == 0x113) && !(val & 0x80)) {
start_blit_op:
                atirage_start_fill(atirage);
                atirage_log("%i %i %i %i %i %08x\n", (atirage->dst_height_width & 0x7ff), (atirage->dst_height_width & 0x7ff0000),
                           ((atirage->dp_src & 7) != SRC_HOST), (((atirage->dp_src >> 8) & 7) != SRC_HOST),
                           (((atirage->dp_src >> 16) & 3) != MONO_SRC_HOST), atirage->dp_src);
                if ((atirage->dst_height_width & 0x7ff) && (atirage->dst_height_width & 0x7ff0000) && ((atirage->dp_src & 7) != SRC_HOST) && (((atirage->dp_src >> 8) & 7) != SRC_HOST) && (((atirage->dp_src >> 16) & 3) != MONO_SRC_HOST))
                    atirage_blit(0, -1, atirage);
            }
            break;

        case 0x2ec ... 0x2ef:
            WRITE8(addr ^ 2, atirage->dst_height_width, val);
            if ((addr & 0x3ff) == 0x2ef) {
                goto start_blit_op;
            }
            break;
        case 0x120 ... 0x123: /* 0_48h DST_BRES_LNTH */
        case 0x144 ... 0x147: /* 0_51h DST_BRES_LNTH Rage alias, also known as LEAD_BRES_LNTH */
            /* In GT, if bit 31 is clear and bit 15 is set, preserve TRAIL_X.
             * This behaviour MUST be tested on real hardware.
             * Todo: write-only bit behaviour and scaler/3D data DP_SRC behaviour
             */
            ; /* Terrible compile hack, will fix later */
            const uint32_t trail_x = (atirage->type >= MACH64_GTPRO) ? (atirage->dst_height_width & 0x7fff) : (atirage->dst_height_width & 0x1fff);    /* Preserved TRAIL_X value for 0x8000 and NOT 0x80008000 */
            WRITE8(addr, atirage->dst_bres_lnth, val);
            if ((addr & 0x3ff) == 0x123 || (addr & 0x3ff) == 0x147) {
                switch (atirage->dst_bres_lnth & 0x80008000) {
                    case 0x0:         /* Bit 31 clear, bit 15 clear: initiate bresenham line draw operation */
                        atirage->dst_height_width = ((atirage->dst_bres_lnth & 0xffff) << 16) | ((atirage->dst_bres_lnth & 0x7fff0000) >> 16);
                        atirage_start_line(atirage);
                        if ((atirage->dst_bres_lnth & 0x7fff) && ((atirage->dp_src & 7) != SRC_HOST) && (((atirage->dp_src >> 8) & 7) != SRC_HOST) && (((atirage->dp_src >> 16) & 3) != MONO_SRC_HOST))
                            atirage_blit(0, -1, atirage);
                        break;
                    case 0x8000:      /* Bit 31 clear, bit 15 set: initiate trapezoid draw operation, preserve TRAIL_X */
                        atirage->dst_height_width = ((atirage->dst_bres_lnth & 0xffff) << 16) | trail_x;
                        atirage->dst_bres_lnth = (atirage->dst_bres_lnth & ~0x1fff0000) | (trail_x << 16);
                        if (atirage->trapezoid_debug)
                            warning("Trapezoid operation not implemented yet, TRAIL_X not written");
                        atirage_start_trap(atirage);
                        if ((atirage->dst_bres_lnth & 0x7fff) && ((atirage->dp_src & 7) != SRC_HOST) && (((atirage->dp_src >> 8) & 7) != SRC_HOST) && (((atirage->dp_src >> 16) & 3) != MONO_SRC_HOST))
                            atirage_blit(0, -1, atirage);
                        break;
                    case 0x80000000:  /* Bit 31 set, bit 15 clear: no operation done, load TRAIL_X and DST_BRES_LNTH */
                        atirage->dst_height_width = ((atirage->dst_bres_lnth & 0xffff) << 16) | ((atirage->dst_bres_lnth & 0x7fff0000) >> 16);
                        break; 
                    case 0x80008000:  /* Bit 31 set, bit 15 set: initiate trapezoid draw operation, write TRAIL_X */ 
                        atirage->dst_height_width = ((atirage->dst_bres_lnth & 0xffff) << 16) | ((atirage->type >= MACH64_GTPRO) ? (atirage->dst_height_width & 0x7fff) : (atirage->dst_height_width & 0x1fff));
                        if (atirage->trapezoid_debug)
                            warning("Trapezoid operation not implemented yet, TRAIL_X written");
                        atirage_start_trap(atirage);
                        if ((atirage->dst_bres_lnth & 0x7fff) && ((atirage->dp_src & 7) != SRC_HOST) && (((atirage->dp_src >> 8) & 7) != SRC_HOST) && (((atirage->dp_src >> 16) & 3) != MONO_SRC_HOST))
                            atirage_blit(0, -1, atirage);
                        break;
                }
            }
            break;
        case 0x124 ... 0x127:
            WRITE8(addr, atirage->dst_bres_err, val);
            break;
        case 0x128 ... 0x12b:
            WRITE8(addr, atirage->dst_bres_inc, val);
            break;
        case 0x12c ... 0x12f:
            WRITE8(addr, atirage->dst_bres_dec, val);
            break;
        case 0x130 ... 0x133:
            WRITE8(addr, atirage->dst_cntl, val);
            break;
        case 0x138 ... 0x13b: /* 0_4Eh TRAIL_BRES_ERR, Rage only*/
            WRITE8(addr, atirage->trail_bres_err, val);
            break;
        case 0x13c ... 0x13f: /* 0_4Fh TRAIL_BRES_INC, Rage only*/
            WRITE8(addr, atirage->trail_bres_inc, val);
            break;
        case 0x140 ... 0x143: /* 0_50h TRAIL_BRES_DEC, Rage only*/
            WRITE8(addr, atirage->trail_bres_dec, val);
            break;
        case 0x148 ... 0x14b:
            warning("Attempt to write to Z_OFF_PITCH addr=%03X,val=%02X",addr,val);
            WRITE8(addr, atirage->z_off_pitch, val);
            break;
        case 0x14c ... 0x14f:
            warning("Attempt to write to Z_CNTL addr=%03X,val=%02X",addr,val);
            WRITE8(addr, atirage->z_cntl, val);
            break;
        case 0x180 ... 0x183:
            WRITE8(addr, atirage->src_off_pitch, val);
            break;
        case 0x184 ... 0x185:
            WRITE8(addr, atirage->src_y_x, val);
            break;
        case 0x188 ... 0x189:
            WRITE8(addr + 2, atirage->src_y_x, val);
            break;
        case 0x18c ... 0x18f:
            WRITE8(addr, atirage->src_y_x, val);
            break;
        case 0x190 ... 0x191:
            WRITE8(addr + 2, atirage->src_height1_width1, val);
            break;
        case 0x194 ... 0x195:
            WRITE8(addr, atirage->src_height1_width1, val);
            break;
        case 0x198 ... 0x19b:
            WRITE8(addr, atirage->src_height1_width1, val);
            break;
        case 0x19c ... 0x19d:
            WRITE8(addr, atirage->src_y_x_start, val);
            break;
        case 0x1a0 ... 0x1a1:
            WRITE8(addr + 2, atirage->src_y_x_start, val);
            break;
        case 0x1a4 ... 0x1a7:
            WRITE8(addr, atirage->src_y_x_start, val);
            break;
        case 0x1a8 ... 0x1a9:
            WRITE8(addr + 2, atirage->src_height2_width2, val);
            break;
        case 0x1ac ... 0x1ad:
            WRITE8(addr, atirage->src_height2_width2, val);
            break;
        case 0x1b0 ... 0x1b3:
            WRITE8(addr, atirage->src_height2_width2, val);
            break;
        case 0x1b4 ... 0x1b7:
            WRITE8(addr, atirage->src_cntl, val);
#ifdef DMA_BM
            if (atirage->src_cntl & (1 << 9))
                pclog("Bus master enabled\n");
            else
                pclog("Bus master disabled\n");
#endif
            break;

        case 0x1c0 ... 0x1c3:
            warning("Attempt to write to SCALE_Y_OFF or TEX_0_OFF addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->tex_0_off, val);
            break;
        case 0x1c4 ... 0x1c7:
            warning("Attempt to write to TEX_1_OFF addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->tex_1_off, val);
            break;
        case 0x1c8 ... 0x1cb:
            warning("Attempt to write to TEX_2_OFF addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->tex_2_off, val);
            break;
        case 0x1cc ... 0x1cf:
            warning("Attempt to write to TEX_3_OFF addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->tex_3_off, val);
            break;
        case 0x1d0 ... 0x1d3:
            warning("Attempt to write to TEX_4_OFF addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->tex_4_off, val);
            break;
        case 0x1d4 ... 0x1d7:
            warning("Attempt to write to TEX_5_OFF addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->tex_5_off, val);
            break;
        case 0x1d8 ... 0x1db:
            warning("Attempt to write to TEX_6_OFF addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->tex_6_off, val);
            break;
        case 0x1dc ... 0x1df:
            warning("Attempt to write to SCALE_WIDTH or TEX_7_OFF addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->tex_7_off, val);
            break;
        case 0x1e0 ... 0x1e3:
            warning("Attempt to write to SCALE_HEIGHT or TEX_8_OFF addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->tex_8_off, val);
            break;
        case 0x1e4 ... 0x1e7:
            warning("Attempt to write to TEX_9_OFF addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->tex_9_off, val);
            break;
        case 0x1e8 ... 0x1eb:
            warning("Attempt to write to TEX_10_OFF addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->tex_10_off, val);
            break;
        case 0x1ec ... 0x1ef:
        case 0x350 ... 0x353: /* 0_7Bh is aliased to 0_D4h */
            warning("Attempt to write to S_Y_INC or SCALE_Y_PITCH addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->s_y_inc, val);
            break;
        case 0x1f0 ... 0x1f3:
        case 0x3c0 ... 0x3c3: /* 0_7Ch is aliased to 0_F0h */
            warning("Attempt to write to SCALE_X_INC or RED_X_INC addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->red_x_inc, val);
            break;
        case 0x1f4 ... 0x1f7:
        case 0x3cc ... 0x3cf: /* 0_7Dh is aliased to 0_F3h */
            warning("Attempt to write to SCALE_Y_INC or GREEN_X_INC addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->green_x_inc, val);
            break;
        case 0x1f8 ... 0x1fb:
            warning("Attempt to write to SCALE_VACC addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->scale_vacc, val);
            break;
        case 0x1fc ... 0x1ff:
            warning("Attempt to write to SCALE_3D_CNTL addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->scale_3d_cntl, val);
            atirage->scaler_active = (atirage->scale_3d_cntl & SCALE_3D_FCN) == 0x40 ? 1 : 0; 
            break;
        case 0x200 ... 0x23f:
            atirage_blit(val, 8, atirage);
            break;
        case 0x240 ... 0x243:
            WRITE8(addr, atirage->host_cntl, val);
            break;
        case 0x280 ... 0x283:
            WRITE8(addr, atirage->pat_reg0, val);
            break;
        case 0x284 ... 0x287:
            WRITE8(addr, atirage->pat_reg1, val);
            break;
        case 0x288 ... 0x28b:
            WRITE8(addr, atirage->pat_cntl, val);
            break;
        case 0x2a0 ... 0x2a1:
        case 0x2a8 ... 0x2a9:
            WRITE8(addr, atirage->sc_left_right, val);
            break;
        case 0x2a4 ... 0x2a5: // doesn't seem right.
            addr += 2;
            fallthrough;
        case 0x2aa ... 0x2ab:
            WRITE8(addr, atirage->sc_left_right, val);
            break;
        case 0x2ac ... 0x2ad:
        case 0x2b4 ... 0x2b5:
            WRITE8(addr, atirage->sc_top_bottom, val);
            break;
        case 0x2b0 ... 0x2b1:
            addr += 2;
            fallthrough;
        case 0x2b6 ... 0x2b7:
            WRITE8(addr, atirage->sc_top_bottom, val);
            break;
        case 0x2c0 ... 0x2c3:
            WRITE8(addr, atirage->dp_bkgd_clr, val);
            break;
        case 0x2c4 ... 0x2c7:
            WRITE8(addr, atirage->dp_frgd_clr, val);
            break;
        case 0x2c8 ... 0x2cb:
            WRITE8(addr, atirage->write_mask, val);
            break;
        case 0x2cc ... 0x2cf:
            WRITE8(addr, atirage->chain_mask, val);
            break;
        case 0x2d0 ... 0x2d3:
            WRITE8(addr, atirage->dp_pix_width, val);
            break;
        case 0x2d4 ... 0x2d7:
            WRITE8(addr, atirage->dp_mix, val);
            break;
        case 0x2d8 ... 0x2db:
            WRITE8(addr, atirage->dp_src, val);
            break;
        case 0x2fc ... 0x2ff:
            WRITE8(addr, atirage->dp_set_gui_engine, val);
            atirage_recalc_dp_set_engine(atirage);
            break;
        case 0x300 ... 0x303:
            WRITE8(addr, atirage->clr_cmp_clr, val);
            break;
        case 0x304 ... 0x307:
            WRITE8(addr, atirage->clr_cmp_mask, val);
            break;
        case 0x308 ... 0x30b:
            WRITE8(addr, atirage->clr_cmp_cntl, val);
            break;
        case 0x320 ... 0x323:
            WRITE8(addr, atirage->context_mask, val);
            break;
        case 0x330 ... 0x331:
            WRITE8(addr, atirage->dst_cntl, val);
            break;
        case 0x332:
            WRITE8(addr - 2, atirage->src_cntl, val);
            break;
        case 0x333:
            WRITE8(addr - 3, atirage->pat_cntl, val & 7);
            if (val & 0x10)
                atirage->host_cntl |= HOST_BYTE_ALIGN;
            else
                atirage->host_cntl &= ~HOST_BYTE_ALIGN;
            break;

        case 0x340 ... 0x343:
            warning("Attempt to write to S_X_INC2 addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->s_x_inc2, val);
            break;
        case 0x344 ... 0x347:
            warning("Attempt to write to S_Y_INC2 addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->s_y_inc2, val);
            break;
        case 0x348 ... 0x34b:
            warning("Attempt to write to S_XY_INC2 addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->s_xy_inc2, val);
            break;
        case 0x34c ... 0x34f:
            warning("Attempt to write to S_XINC_START addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->s_xinc_start, val);
            break;
        case 0x354 ... 0x357:
            warning("Attempt to write to S_START addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->s_start, val);
            break;
        case 0x358 ... 0x35b:
            warning("Attempt to write to T_X_INC2 addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->t_x_inc2, val);
            break;
        case 0x35c ... 0x35f:
            warning("Attempt to write to T_Y_INC2 addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->t_y_inc2, val);
            break;
        case 0x360 ... 0x363:
            warning("Attempt to write to T_XY_INC2 addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->t_xy_inc2, val);
            break;
        case 0x364 ... 0x367:
            warning("Attempt to write to T_XINC_START addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->t_xinc_start, val);
            break;
        case 0x368 ... 0x36b:
            warning("Attempt to write to T_Y_INC addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->t_y_inc, val);
            break;
        case 0x36c ... 0x36f:
            warning("Attempt to write to T_START addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->t_start, val);
            break;
        case 0x370 ... 0x373:
            warning("Attempt to write to TEX_SIZE_PITCH addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->tex_size_pitch, val);
            break;
        case 0x3c4 ... 0x3c7:
            warning("Attempt to write to RED_Y_INC addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->red_y_inc, val);
            break;
        case 0x3c8 ... 0x3cb:
            warning("Attempt to write to SCALE_HACC or RED_START addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->red_start, val);
            break;
        case 0x3d0 ... 0x3d3:
            warning("Attempt to write to GREEN_Y_INC addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->green_y_inc, val);
            break;
        case 0x3d4 ... 0x3d7:
            warning("Attempt to write to GREEN_START addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->green_start, val);
            break;
        case 0x3d8 ... 0x3db:
            warning("Attempt to write to SCALE_XUV_INC or BLUE_X_INC addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->blue_x_inc, val);
            break;
        case 0x3dc ... 0x3df:
            warning("Attempt to write to BLUE_Y_INC addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->blue_y_inc, val);
            break;
        case 0x3e0 ... 0x3e3:
            warning("Attempt to write to SCALE_UV_HACC or BLUE_START addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->blue_start, val);
            break;
        case 0x3e4 ... 0x3e7:
            warning("Attempt to write to Z_X_INC addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->z_x_inc, val);
            break;
        case 0x3e8 ... 0x3eb:
            warning("Attempt to write to Z_Y_INC addr=%08X, val=%02X",addr,val);
            WRITE8(addr, atirage->z_y_inc, val);
            break;
        case 0x3ec ... 0x3ef:
            warning("Attempt to write to Z_START addr=%08X, val=%02X",addr,val);
            WRITE8(addr, atirage->z_start, val);
            break;
        case 0x3f0 ... 0x3f3:
            warning("Attempt to write to ALPHA_X_INC or FOG_X_INC addr=%08X,val=%02X",addr,val);
            WRITE8(addr, atirage->alpha_x_inc, val);
            break;
        case 0x3f4 ... 0x3f7:
            warning("Attempt to write to ALPHA_Y_INC or FOG_Y_INC addr=%08X, val=%02X",addr,val);
            WRITE8(addr, atirage->alpha_y_inc, val);
            break;
        case 0x3f8 ... 0x3fb:
            warning("Attempt to write to ALPHA_START or FOG_START addr=%08X, val=%02X",addr,val);
            WRITE8(addr, atirage->alpha_start, val);
            break;


        default:
            break;
    }
}
static void
atirage_accel_write_fifo_w(atirage_t *atirage, uint32_t addr, uint16_t val)
{
    // if the address is word aligned and we are between 200-23e, do a 16 pixel blit.

    addr &= 0x3fe;

    if (addr & 2
    && (addr >= 0x200 && addr <= 0x23e))
        atirage_blit(val, 16, atirage);
    else
    {
        switch (addr & 0x3fe) {
            case 0x2fc:
                atirage->dp_set_gui_engine |= (atirage->dp_set_gui_engine & 0xffff0000) | val;
                atirage_recalc_dp_set_engine(atirage);
                break;
            case 0x2fe:
                atirage->dp_set_gui_engine |= (atirage->dp_set_gui_engine & 0xffff) | (val << 16);
                atirage_recalc_dp_set_engine(atirage);
                break;
            case 0x32c:
                atirage->context_load_cntl = (atirage->context_load_cntl & 0xffff0000) | val;
                break;
            case 0x32e:
                atirage->context_load_cntl = (atirage->context_load_cntl & 0x0000ffff) | (val << 16);
                if (val & 0x30000)
                    atirage_load_context(atirage);
                break;
            default:
                atirage_accel_write_fifo(atirage, addr, val);
                atirage_accel_write_fifo(atirage, addr + 1, val >> 8);
                break;
            }
    }
    
}
static void
atirage_accel_write_fifo_l(atirage_t *atirage, uint32_t addr, uint32_t val)
{
    addr &= 0x3fc;

    if (addr >= 0x200 && addr <= 0x23c)
    {
        if (atirage->accel.source_host || (atirage->dp_pix_width & DP_BYTE_PIX_ORDER))
            atirage_blit(val, 32, atirage);
        else
            atirage_blit(((val & 0xff000000) >> 24) | ((val & 0x00ff0000) >> 8) | ((val & 0x0000ff00) << 8) | ((val & 0x000000ff) << 24), 32, atirage);
    }
    else
    {
        switch (addr & 0x3fc) {
            case 0x32c:
                atirage->context_load_cntl = val;
                if (val & 0x30000)
                    atirage_load_context(atirage);
                break;
            case 0x2fc:
                atirage->dp_set_gui_engine = val;
                atirage_recalc_dp_set_engine(atirage);
                break;
            default:
                atirage_accel_write_fifo_w(atirage, addr, val);
                atirage_accel_write_fifo_w(atirage, addr + 2, val >> 16);
                break;
        }
    }
}

#ifdef DMA_BM
static void
run_dma(atirage_t *atirage)
{
    int words_transferred = 0;

    thread_wait_mutex(atirage->dma.lock);
    thread_release_mutex(atirage->dma.lock);

}
#endif

__inline void
atirage_wake_fifo_thread(atirage_t *atirage)
{
    thread_set_event(atirage->wake_fifo_thread); /*Wake up FIFO thread if moving from idle*/
}

void
atirage_wait_fifo_idle(atirage_t *atirage)
{
    while (!FIFO_EMPTY) {
        atirage_wake_fifo_thread(atirage);
        thread_wait_event(atirage->fifo_not_full_event, 1);
    }
}

void
atirage_fifo_thread(void *param)
{
    atirage_t *atirage = (atirage_t *) param;

    while (atirage->thread_run) {
        thread_set_event(atirage->fifo_not_full_event);
        thread_wait_event(atirage->wake_fifo_thread, -1);
        thread_reset_event(atirage->wake_fifo_thread);
        atirage->blitter_busy = 1;
        while (!FIFO_EMPTY) {
            uint64_t      start_time = plat_timer_read();
            uint64_t      end_time;
            fifo_entry_t *fifo = &atirage->fifo[atirage->fifo_read_idx & FIFO_MASK];
            uint32_t      val  = fifo->val;

            switch (fifo->addr_type & FIFO_TYPE) {
                case FIFO_WRITE_BYTE:
                    atirage_accel_write_fifo(atirage, fifo->addr_type & FIFO_ADDR, val);
                    break;
                case FIFO_WRITE_WORD:
                    atirage_accel_write_fifo_w(atirage, fifo->addr_type & FIFO_ADDR, val);
                    break;
                case FIFO_WRITE_DWORD:
                    atirage_accel_write_fifo_l(atirage, fifo->addr_type & FIFO_ADDR, val);
                    break;
                default:
                    break;
            }

            atirage->fifo_read_idx++;
            fifo->addr_type = FIFO_INVALID;

            if (FIFO_ENTRIES > 0xe000)
                thread_set_event(atirage->fifo_not_full_event);

            end_time = plat_timer_read();
            atirage->blitter_time += end_time - start_time;
        }
#ifdef DMA_BM
        run_dma(atirage);
#endif
        atirage->blitter_busy = 0;
    }
}

void
atirage_queue(atirage_t *atirage, uint32_t addr, uint32_t val, uint32_t type)
{
    fifo_entry_t *fifo = &atirage->fifo[atirage->fifo_write_idx & FIFO_MASK];

    // Before me, there was some code that checked if the address was 0x11b (if a byte), 0x11a (if a word), or 0x118 (if a dword), and only fired the FIFO thread
    // if there were 16 or more entries in the FIFO. It was introduced in a commit on 8/21/2024 with no discussion that I can find even related to it. 
    // It didn't break anything to remove it and I can't think of any design reason for it to exist.

    if (FIFO_FULL) {
        thread_reset_event(atirage->fifo_not_full_event);
        if (FIFO_FULL)
            thread_wait_event(atirage->fifo_not_full_event, -1); /*Wait for room in ringbuffer*/
    }

    fifo->val       = val;
    fifo->addr_type = (addr & FIFO_ADDR) | type;

    atirage->fifo_write_idx++;

    if (FIFO_ENTRIES > 0xe000)
        atirage_wake_fifo_thread(atirage);
    if (FIFO_ENTRIES > 0xe000 || FIFO_ENTRIES < 8)
        atirage_wake_fifo_thread(atirage);
}

#define READ(addr, dat, width)                                                         \
    if (width == 0)                                                                    \
        dat = svga->vram[((addr)) & atirage->vram_mask];                                \
    else if (width == 1)                                                               \
        dat = *(uint16_t *) &svga->vram[((addr) << 1) & atirage->vram_mask];            \
    else if (width == 2)                                                               \
        dat = *(uint32_t *) &svga->vram[((addr) << 2) & atirage->vram_mask];            \
    else if (atirage->dp_pix_width & DP_BYTE_PIX_ORDER)                                 \
        dat = (svga->vram[((addr) >> 3) & atirage->vram_mask] >> ((addr) &7)) & 1;      \
    else                                                                               \
        dat = (svga->vram[((addr) >> 3) & atirage->vram_mask] >> (7 - ((addr) &7))) & 1;

/* Mix function 0x17 is reserved in 3D Rage. */
#define MIX                                                      \
    switch (mix ? atirage->accel.mix_fg : atirage->accel.mix_bg) { \
        case 0x0:                                                \
            dest_dat = ~dest_dat;                                \
            break;                                               \
        case 0x1:                                                \
            dest_dat = 0;                                        \
            break;                                               \
        case 0x2:                                                \
            dest_dat = 0xffffffff;                               \
            break;                                               \
        case 0x3:                                                \
            dest_dat = dest_dat;                                 \
            break;                                               \
        case 0x4:                                                \
            dest_dat = ~src_dat;                                 \
            break;                                               \
        case 0x5:                                                \
            dest_dat = src_dat ^ dest_dat;                       \
            break;                                               \
        case 0x6:                                                \
            dest_dat = ~(src_dat ^ dest_dat);                    \
            break;                                               \
        case 0x7:                                                \
            dest_dat = src_dat;                                  \
            break;                                               \
        case 0x8:                                                \
            dest_dat = ~(src_dat & dest_dat);                    \
            break;                                               \
        case 0x9:                                                \
            dest_dat = ~src_dat | dest_dat;                      \
            break;                                               \
        case 0xa:                                                \
            dest_dat = src_dat | ~dest_dat;                      \
            break;                                               \
        case 0xb:                                                \
            dest_dat = src_dat | dest_dat;                       \
            break;                                               \
        case 0xc:                                                \
            dest_dat = src_dat & dest_dat;                       \
            break;                                               \
        case 0xd:                                                \
            dest_dat = src_dat & ~dest_dat;                      \
            break;                                               \
        case 0xe:                                                \
            dest_dat = ~src_dat & dest_dat;                      \
            break;                                               \
        case 0xf:                                                \
            dest_dat = ~(src_dat | dest_dat);                    \
            break;                                               \
    }

#define WRITE(addr, width)                                                                                  \
    if (width == 0) {                                                                                       \
        svga->vram[(addr) &atirage->vram_mask]                = dest_dat;                                    \
        svga->changedvram[((addr) &atirage->vram_mask) >> 12] = svga->monitor->mon_changeframecount;         \
    } else if (width == 1) {                                                                                \
        *(uint16_t *) &svga->vram[((addr) << 1) & atirage->vram_mask] = dest_dat;                            \
        svga->changedvram[(((addr) << 1) & atirage->vram_mask) >> 12] = svga->monitor->mon_changeframecount; \
    } else if (width == 2) {                                                                                \
        *(uint32_t *) &svga->vram[((addr) << 2) & atirage->vram_mask] = dest_dat;                            \
        svga->changedvram[(((addr) << 2) & atirage->vram_mask) >> 12] = svga->monitor->mon_changeframecount; \
    } else {                                                                                                \
        if (dest_dat & 1) {                                                                                 \
            if (atirage->dp_pix_width & DP_BYTE_PIX_ORDER)                                                   \
                svga->vram[((addr) >> 3) & atirage->vram_mask] |= 1 << ((addr) &7);                          \
            else                                                                                            \
                svga->vram[((addr) >> 3) & atirage->vram_mask] |= 1 << (7 - ((addr) &7));                    \
        } else {                                                                                            \
            if (atirage->dp_pix_width & DP_BYTE_PIX_ORDER)                                                   \
                svga->vram[((addr) >> 3) & atirage->vram_mask] &= ~(1 << ((addr) &7));                       \
            else                                                                                            \
                svga->vram[((addr) >> 3) & atirage->vram_mask] &= ~(1 << (7 - ((addr) &7)));                 \
        }                                                                                                   \
        svga->changedvram[(((addr) >> 3) & atirage->vram_mask) >> 12] = svga->monitor->mon_changeframecount; \
    }

void
atirage_start_fill(atirage_t *atirage)
{
    atirage->accel.dst_pix_width  = atirage->dp_pix_width & 7;
    atirage->accel.src_pix_width  = (atirage->dp_pix_width >> 8) & 7;
    atirage->accel.host_pix_width = (atirage->dp_pix_width >> 16) & 7;

    atirage->accel.dst_size  = atirage_width[atirage->accel.dst_pix_width];
    atirage->accel.src_size  = atirage_width[atirage->accel.src_pix_width];
    atirage->accel.host_size = atirage_width[atirage->accel.host_pix_width];

    atirage->accel.dst_x = 0;
    atirage->accel.dst_y = 0;

    atirage->accel.dst_x_start = (atirage->dst_y_x >> 16) & 0xfff;
    if ((atirage->dst_y_x >> 16) & 0x1000)
        atirage->accel.dst_x_start |= ~0xfff;
    atirage->accel.dst_y_start = atirage->dst_y_x & 0x3fff;
    if (atirage->dst_y_x & 0x4000)
        atirage->accel.dst_y_start |= ~0x3fff;

    atirage->accel.dst_width  = (atirage->dst_height_width >> 16) & 0x1fff;
    atirage->accel.dst_height = atirage->dst_height_width & 0x1fff;

    if ((((atirage->dp_src >> 16) & 7) == MONO_SRC_BLITSRC) &&
        ((atirage->src_cntl & (SRC_LINEAR_EN | SRC_BYTE_ALIGN)) == (SRC_LINEAR_EN | SRC_BYTE_ALIGN))) {
        if (atirage->accel.dst_width & 7)
            atirage->accel.dst_width = (atirage->accel.dst_width & ~7) + 8;
    }

    atirage->accel.x_count  = atirage->accel.dst_width;
    atirage->accel.xx_count = 0;

    atirage->accel.src_x = 0;
    atirage->accel.src_y = 0;

    atirage->accel.src_x_start = (atirage->src_y_x >> 16) & 0xfff;
    if ((atirage->src_y_x >> 16) & 0x1000)
        atirage->accel.src_x_start |= ~0xfff;
    atirage->accel.src_y_start = atirage->src_y_x & 0x3fff;
    if (atirage->src_y_x & 0x4000)
        atirage->accel.src_y_start |= ~0x3fff;

    if (atirage->src_cntl & SRC_LINEAR_EN)
        atirage->accel.src_x_count = 0x7ffffff; /*Essentially infinite*/
    else
        atirage->accel.src_x_count = (atirage->src_height1_width1 >> 16) & 0x7fff;
    if (!(atirage->src_cntl & SRC_PATT_EN))
        atirage->accel.src_y_count = 0x7ffffff; /*Essentially infinite*/
    else
        atirage->accel.src_y_count = atirage->src_height1_width1 & 0x1fff;

    atirage->accel.src_width1  = (atirage->src_height1_width1 >> 16) & 0x7fff;
    atirage->accel.src_height1 = atirage->src_height1_width1 & 0x1fff;
    atirage->accel.src_width2  = (atirage->src_height2_width2 >> 16) & 0x7fff;
    atirage->accel.src_height2 = atirage->src_height2_width2 & 0x1fff;

    atirage_log("src %i %i  %i %i  %08X %08X\n", atirage->accel.src_x_count,
               atirage->accel.src_y_count,
               atirage->accel.src_width1,
               atirage->accel.src_height1,
               atirage->src_height1_width1,
               atirage->src_height2_width2);

    atirage->accel.src_pitch  = (atirage->src_off_pitch >> 22) << 3;
    atirage->accel.src_offset = (atirage->src_off_pitch & 0xfffff) << 3;

    atirage->accel.dst_pitch  = (atirage->dst_off_pitch >> 22) << 3;
    atirage->accel.dst_offset = (atirage->dst_off_pitch & 0xfffff) << 3;

    atirage->accel.mix_fg = (atirage->dp_mix >> 16) & 0x1f;
    atirage->accel.mix_bg = atirage->dp_mix & 0x1f;

    atirage->accel.source_bg  = atirage->dp_src & 7;
    atirage->accel.source_fg  = (atirage->dp_src >> 8) & 7;
    atirage->accel.source_mix = (atirage->dp_src >> 16) & 7;

    if (atirage->accel.src_size == WIDTH_1BIT)
        atirage->accel.src_offset <<= 3;
    else
        atirage->accel.src_offset >>= atirage->accel.src_size;

    if (atirage->accel.dst_size == WIDTH_1BIT)
        atirage->accel.dst_offset <<= 3;
    else
        atirage->accel.dst_offset >>= atirage->accel.dst_size;

    atirage->accel.xinc = (atirage->dst_cntl & DST_X_DIR) ? 1 : -1;
    atirage->accel.yinc = (atirage->dst_cntl & DST_Y_DIR) ? 1 : -1;

    atirage->accel.source_host = ((atirage->dp_src & 7) == SRC_HOST) || (((atirage->dp_src >> 8) & 7) == SRC_HOST);

    if (atirage->pat_cntl & 1) {
        for (uint8_t y = 0; y < 8; y++) {
            for (uint8_t x = 0; x < 8; x++) {
                uint32_t temp                   = (y & 4) ? atirage->pat_reg1 : atirage->pat_reg0;
                atirage->accel.pattern[y][7 - x] = (temp >> (x + ((y & 3) << 3))) & 1;
            }
        }
    }

    if (atirage->pat_cntl & 2) {
        atirage->accel.pattern_clr4x2[0][0] = (atirage->pat_reg0 & 0xff);
        atirage->accel.pattern_clr4x2[0][1] = ((atirage->pat_reg0 >> 8) & 0xff);
        atirage->accel.pattern_clr4x2[0][2] = ((atirage->pat_reg0 >> 16) & 0xff);
        atirage->accel.pattern_clr4x2[0][3] = ((atirage->pat_reg0 >> 24) & 0xff);
        atirage->accel.pattern_clr4x2[1][0] = (atirage->pat_reg1 & 0xff);
        atirage->accel.pattern_clr4x2[1][1] = ((atirage->pat_reg1 >> 8) & 0xff);
        atirage->accel.pattern_clr4x2[1][2] = ((atirage->pat_reg1 >> 16) & 0xff);
        atirage->accel.pattern_clr4x2[1][3] = ((atirage->pat_reg1 >> 24) & 0xff);
    }

    if (atirage->pat_cntl & 4) {
        atirage->accel.pattern_clr8x1[0] = (atirage->pat_reg0 & 0xff);
        atirage->accel.pattern_clr8x1[1] = ((atirage->pat_reg0 >> 8) & 0xff);
        atirage->accel.pattern_clr8x1[2] = ((atirage->pat_reg0 >> 16) & 0xff);
        atirage->accel.pattern_clr8x1[3] = ((atirage->pat_reg0 >> 24) & 0xff);
        atirage->accel.pattern_clr8x1[4] = (atirage->pat_reg1 & 0xff);
        atirage->accel.pattern_clr8x1[5] = ((atirage->pat_reg1 >> 8) & 0xff);
        atirage->accel.pattern_clr8x1[6] = ((atirage->pat_reg1 >> 16) & 0xff);
        atirage->accel.pattern_clr8x1[7] = ((atirage->pat_reg1 >> 24) & 0xff);
    }

    if ((atirage->src_cntl & SRC_8x8x8_BRUSH) && !(atirage->src_cntl & SRC_8x8x8_BRUSH_LOADED))
    {
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                atirage->accel.pattern_clr8x8[y][x] = atirage->svga.vram[(atirage->accel.src_offset & ~7) + (y * 8) + x];
    }

    atirage->accel.sc_left   = atirage->sc_left_right & 0x1fff;
    atirage->accel.sc_right  = (atirage->sc_left_right >> 16) & 0x1fff;
    atirage->accel.sc_top    = atirage->sc_top_bottom & 0x7fff;
    atirage->accel.sc_bottom = (atirage->sc_top_bottom >> 16) & 0x7fff;

    atirage->accel.dp_frgd_clr = atirage->dp_frgd_clr;
    atirage->accel.dp_bkgd_clr = atirage->dp_bkgd_clr;
    atirage->accel.write_mask  = atirage->write_mask;

    atirage->accel.clr_cmp_clr  = atirage->clr_cmp_clr & atirage->clr_cmp_mask;
    atirage->accel.clr_cmp_mask = atirage->clr_cmp_mask;
    atirage->accel.clr_cmp_fn   = atirage->clr_cmp_cntl & 7;
    atirage->accel.clr_cmp_src  = atirage->clr_cmp_cntl & (1 << 24);

    atirage->accel.poly_draw = 0;

    atirage->accel.busy = 1;

    atirage->accel.op = OP_RECT;
}

void
atirage_start_line(atirage_t *atirage)
{
    atirage->accel.dst_x = (atirage->dst_y_x >> 16) & 0xfff;
    if ((atirage->dst_y_x >> 16) & 0x1000)
        atirage->accel.dst_x |= ~0xfff;
    atirage->accel.dst_y = atirage->dst_y_x & 0x3fff;
    if (atirage->dst_y_x & 0x4000)
        atirage->accel.dst_y |= ~0x3fff;

    atirage->accel.src_x = (atirage->src_y_x >> 16) & 0xfff;
    if ((atirage->src_y_x >> 16) & 0x1000)
        atirage->accel.src_x |= ~0xfff;
    atirage->accel.src_y = atirage->src_y_x & 0x3fff;
    if (atirage->src_y_x & 0x4000)
        atirage->accel.src_y |= ~0x3fff;

    atirage->accel.src_pitch  = (atirage->src_off_pitch >> 22) << 3;
    atirage->accel.src_offset = (atirage->src_off_pitch & 0xfffff) << 3;

    atirage->accel.dst_pitch  = (atirage->dst_off_pitch >> 22) << 3;
    atirage->accel.dst_offset = (atirage->dst_off_pitch & 0xfffff) << 3;

    atirage->accel.mix_fg = (atirage->dp_mix >> 16) & 0x1f;
    atirage->accel.mix_bg = atirage->dp_mix & 0x1f;

    atirage->accel.source_bg  = atirage->dp_src & 7;
    atirage->accel.source_fg  = (atirage->dp_src >> 8) & 7;
    atirage->accel.source_mix = (atirage->dp_src >> 16) & 7;

    atirage->accel.dst_pix_width  = atirage->dp_pix_width & 7;
    atirage->accel.src_pix_width  = (atirage->dp_pix_width >> 8) & 7;
    atirage->accel.host_pix_width = (atirage->dp_pix_width >> 16) & 7;

    atirage->accel.dst_size  = atirage_width[atirage->accel.dst_pix_width];
    atirage->accel.src_size  = atirage_width[atirage->accel.src_pix_width];
    atirage->accel.host_size = atirage_width[atirage->accel.host_pix_width];

    if (atirage->accel.src_size == WIDTH_1BIT)
        atirage->accel.src_offset <<= 3;
    else
        atirage->accel.src_offset >>= atirage->accel.src_size;

    if (atirage->accel.dst_size == WIDTH_1BIT)
        atirage->accel.dst_offset <<= 3;
    else
        atirage->accel.dst_offset >>= atirage->accel.dst_size;

    atirage->accel.source_host = ((atirage->dp_src & 7) == SRC_HOST) || (((atirage->dp_src >> 8) & 7) == SRC_HOST);

    if (atirage->pat_cntl & 1) {
        for (uint8_t y = 0; y < 8; y++) {
            for (uint8_t x = 0; x < 8; x++) {
                uint32_t temp                   = (y & 4) ? atirage->pat_reg1 : atirage->pat_reg0;
                atirage->accel.pattern[y][7 - x] = (temp >> (x + ((y & 3) << 3))) & 1;
            }
        }
    }
    atirage->accel.sc_left   = atirage->sc_left_right & 0x1fff;
    atirage->accel.sc_right  = (atirage->sc_left_right >> 16) & 0x1fff;
    atirage->accel.sc_top    = atirage->sc_top_bottom & 0x7fff;
    atirage->accel.sc_bottom = (atirage->sc_top_bottom >> 16) & 0x7fff;

    atirage->accel.dp_frgd_clr = atirage->dp_frgd_clr;
    atirage->accel.dp_bkgd_clr = atirage->dp_bkgd_clr;
    atirage->accel.write_mask  = atirage->write_mask;

    atirage->accel.x_count = atirage->dst_bres_lnth & 0x7fff;
    atirage->accel.err     = (atirage->dst_bres_err & 0x3ffff) | ((atirage->dst_bres_err & 0x40000) ? 0xfffc0000 : 0);

    atirage->accel.clr_cmp_clr  = atirage->clr_cmp_clr & atirage->clr_cmp_mask;
    atirage->accel.clr_cmp_mask = atirage->clr_cmp_mask;
    atirage->accel.clr_cmp_fn   = atirage->clr_cmp_cntl & 7;
    atirage->accel.clr_cmp_src  = atirage->clr_cmp_cntl & (1 << 24);

    atirage->accel.xinc = (atirage->dst_cntl & DST_X_DIR) ? 1 : -1;
    atirage->accel.yinc = (atirage->dst_cntl & DST_Y_DIR) ? 1 : -1;

    atirage->accel.busy = 1;
    atirage_log("atirage_start_line\n");

    atirage->accel.op = OP_LINE;
}

/* This is where we draw a trapezoid for 3D Rage and later */
void
atirage_start_trap(atirage_t *atirage)
{
    atirage->accel.dst_x = (atirage->dst_y_x >> 16) & 0xfff;
    if ((atirage->dst_y_x >> 16) & 0x1000)
        atirage->accel.dst_x |= ~0xfff;
    atirage->accel.dst_y = atirage->dst_y_x & 0x3fff;
    if (atirage->dst_y_x & 0x4000)
        atirage->accel.dst_y |= ~0x3fff;
    

    if (atirage->type >= MACH64_GTPRO) {
        atirage->accel.trail_x = atirage->dst_height_width & 0x7fff;
    } else {
        atirage->accel.trail_x = atirage->dst_height_width & 0x1fff;
    }

    atirage->accel.trail_err = (atirage->trail_bres_err & 0x3ffff) | ((atirage->trail_bres_err & 0x40000) ? 0xfffc0000 : 0);
    atirage->accel.trail_xinc = (atirage->dst_cntl & TRAIL_X_DIR) ? 1 : -1;

    atirage->accel.src_x = (atirage->src_y_x >> 16) & 0xfff;
    if ((atirage->src_y_x >> 16) & 0x1000)
        atirage->accel.src_x |= ~0xfff;
    atirage->accel.src_y = atirage->src_y_x & 0x3fff;
    if (atirage->src_y_x & 0x4000)
        atirage->accel.src_y |= ~0x3fff;

    atirage->accel.src_pitch  = (atirage->src_off_pitch >> 22) << 3;
    atirage->accel.src_offset = (atirage->src_off_pitch & 0xfffff) << 3;

    atirage->accel.dst_pitch  = (atirage->dst_off_pitch >> 22) << 3;
    atirage->accel.dst_offset = (atirage->dst_off_pitch & 0xfffff) << 3;

    atirage->accel.mix_fg = (atirage->dp_mix >> 16) & 0x1f;
    atirage->accel.mix_bg = atirage->dp_mix & 0x1f;

    atirage->accel.source_bg  = atirage->dp_src & 7;
    atirage->accel.source_fg  = (atirage->dp_src >> 8) & 7;
    atirage->accel.source_mix = (atirage->dp_src >> 16) & 7;

    atirage->accel.dst_pix_width  = atirage->dp_pix_width & 7;
    atirage->accel.src_pix_width  = (atirage->dp_pix_width >> 8) & 7;
    atirage->accel.host_pix_width = (atirage->dp_pix_width >> 16) & 7;

    atirage->accel.dst_size  = atirage_width[atirage->accel.dst_pix_width];
    atirage->accel.src_size  = atirage_width[atirage->accel.src_pix_width];
    atirage->accel.host_size = atirage_width[atirage->accel.host_pix_width];

    if (atirage->accel.src_size == WIDTH_1BIT)
        atirage->accel.src_offset <<= 3;
    else
        atirage->accel.src_offset >>= atirage->accel.src_size;

    if (atirage->accel.dst_size == WIDTH_1BIT)
        atirage->accel.dst_offset <<= 3;
    else
        atirage->accel.dst_offset >>= atirage->accel.dst_size;

    atirage->accel.source_host = ((atirage->dp_src & 7) == SRC_HOST) || (((atirage->dp_src >> 8) & 7) == SRC_HOST);

    if (atirage->pat_cntl & 1) {
        for (uint8_t y = 0; y < 8; y++) {
            for (uint8_t x = 0; x < 8; x++) {
                uint32_t temp                   = (y & 4) ? atirage->pat_reg1 : atirage->pat_reg0;
                atirage->accel.pattern[y][7 - x] = (temp >> (x + ((y & 3) << 3))) & 1;
            }
        }
    }
    atirage->accel.sc_left   = atirage->sc_left_right & 0x1fff;
    atirage->accel.sc_right  = (atirage->sc_left_right >> 16) & 0x1fff;
    atirage->accel.sc_top    = atirage->sc_top_bottom & 0x7fff;
    atirage->accel.sc_bottom = (atirage->sc_top_bottom >> 16) & 0x7fff;

    atirage->accel.dp_frgd_clr = atirage->dp_frgd_clr;
    atirage->accel.dp_bkgd_clr = atirage->dp_bkgd_clr;
    atirage->accel.write_mask  = atirage->write_mask;

    atirage->accel.x_count  = atirage->dst_bres_lnth & 0x7fff;
    atirage->accel.temp_cnt = -1;
    atirage->accel.y_count  = 1;
    atirage->accel.err      = (atirage->dst_bres_err & 0x3ffff) | ((atirage->dst_bres_err & 0x40000) ? 0xfffc0000 : 0);

    atirage->accel.clr_cmp_clr  = atirage->clr_cmp_clr & atirage->clr_cmp_mask;
    atirage->accel.clr_cmp_mask = atirage->clr_cmp_mask;
    atirage->accel.clr_cmp_fn   = atirage->clr_cmp_cntl & 7;
    atirage->accel.clr_cmp_src  = atirage->clr_cmp_cntl & (1 << 24);

    atirage->accel.xinc = (atirage->dst_cntl & DST_X_DIR) ? 1 : -1;
    atirage->accel.yinc = (atirage->dst_cntl & DST_Y_DIR) ? 1 : -1;

    atirage->accel.busy = 1;
    atirage_log("atirage_start_trap\n");

    atirage->accel.op = OP_TRAP;
}

// calculates colour compare function for mach64 blit
int32_t
atirage_blit_calc_cmp_clr(atirage_t* atirage, uint32_t src_dat, uint32_t dest_dat)
{
    int32_t cmp_clr = 0;

    switch (atirage->accel.clr_cmp_fn) {
        case 1: /*TRUE*/
            cmp_clr = 1;
            break;
        case 4: /*DST_CLR != CLR_CMP_CLR*/
            cmp_clr = (((atirage->accel.clr_cmp_src) ? src_dat : dest_dat) & atirage->accel.clr_cmp_mask) != atirage->accel.clr_cmp_clr;
            break;
        case 5: /*DST_CLR == CLR_CMP_CLR*/
            cmp_clr = (((atirage->accel.clr_cmp_src) ? src_dat : dest_dat) & atirage->accel.clr_cmp_mask) == atirage->accel.clr_cmp_clr;
            break;
        default:
            break;
    }

    return cmp_clr; 
}

void
atirage_blit_rect(uint32_t cpu_dat, int count, atirage_t* atirage)
{
    svga_t *svga    = &atirage->svga;
    int     cmp_clr = 0;
    int     mix = 0;

    while (count) {
        uint8_t  write_mask = 0;
        uint32_t src_dat = 0;
        uint32_t dest_dat;
        uint32_t host_dat = 0;
        uint32_t old_dest_dat;
        int      dst_x, dst_y;
        int      src_x, src_y;

        dst_x = (atirage->accel.dst_x + atirage->accel.dst_x_start) & 0xfff;
        dst_y = (atirage->accel.dst_y + atirage->accel.dst_y_start) & 0x3fff;

        if (atirage->src_cntl & SRC_LINEAR_EN)
            src_x = atirage->accel.src_x;
        else
            src_x = (atirage->accel.src_x + atirage->accel.src_x_start) & 0xfff;

        src_y = (atirage->accel.src_y + atirage->accel.src_y_start) & 0x3fff;

        if (atirage->accel.source_host) {
            host_dat = cpu_dat;

            if (atirage->accel.host_size < 2)
                cpu_dat >>= (8 << atirage->accel.host_size); // shift by 8 for a word, 16 for a word, not at all for a dword.

            count -= (8 << atirage->accel.host_size);
        } else
            count--;

        switch (atirage->accel.source_mix) {
            case MONO_SRC_HOST:
                if (atirage->dp_pix_width & DP_BYTE_PIX_ORDER) {
                    mix = cpu_dat & 1;
                    cpu_dat >>= 1;
                } else {
                    mix = cpu_dat >> 0x1f;
                    cpu_dat <<= 1;
                }
                break;
            case MONO_SRC_PAT:
                if (atirage->dst_cntl & DST_24_ROT_EN) {
                    if (!atirage->accel.xx_count)
                        mix = atirage->accel.pattern[dst_y & 7][(dst_x / 3) & 7];
                } else
                    mix = atirage->accel.pattern[dst_y & 7][dst_x & 7];
                break;
            case MONO_SRC_1:
                mix = 1;
                break;
            case MONO_SRC_BLITSRC:
                if (atirage->src_cntl & SRC_LINEAR_EN) {
                    READ(atirage->accel.src_offset + src_x, mix, WIDTH_1BIT);
                } else {
                    READ(atirage->accel.src_offset + (src_y * atirage->accel.src_pitch) + src_x, mix, WIDTH_1BIT);
                }
                break;
            default:
                break;
        }

        if (dst_x >= atirage->accel.sc_left && dst_x <= atirage->accel.sc_right && dst_y >= atirage->accel.sc_top && dst_y <= atirage->accel.sc_bottom) {
            switch (mix ? atirage->accel.source_fg : atirage->accel.source_bg) {
                case SRC_HOST:
                    src_dat = host_dat;
                    break;
                case SRC_BLITSRC:
                    if (atirage->accel.src_size == 0 && atirage->type == MACH64_GTB && (atirage->src_cntl & SRC_8x8x8_BRUSH))
                        src_dat = atirage->accel.pattern_clr8x8[dst_y & 7][dst_x & 7];
                    else
                        READ(atirage->accel.src_offset + (src_y * atirage->accel.src_pitch) + src_x, src_dat, atirage->accel.src_size);
                    break;
                case SRC_FG:
                    if (atirage->dst_cntl & DST_24_ROT_EN) {
                        if (atirage->accel.xinc == -1) {
                            if (atirage->accel.xx_count == 2)
                                src_dat = atirage->accel.dp_frgd_clr & 0xff;
                            else if (atirage->accel.xx_count == 1)
                                src_dat = (atirage->accel.dp_frgd_clr >> 8) & 0xff;
                            else
                                src_dat = (atirage->accel.dp_frgd_clr >> 16) & 0xff;
                        } else {
                            if (atirage->accel.xx_count == 2)
                                src_dat = (atirage->accel.dp_frgd_clr >> 16) & 0xff;
                            else if (atirage->accel.xx_count == 1)
                                src_dat = (atirage->accel.dp_frgd_clr >> 8) & 0xff;
                            else
                                src_dat = atirage->accel.dp_frgd_clr & 0xff;
                        }
                    } else
                        src_dat = atirage->accel.dp_frgd_clr;
                    break;
                case SRC_BG:
                    if (atirage->dst_cntl & DST_24_ROT_EN) {
                        if (atirage->accel.xinc == -1) {
                            if (atirage->accel.xx_count == 2)
                                src_dat = atirage->accel.dp_bkgd_clr & 0xff;
                            else if (atirage->accel.xx_count == 1)
                                src_dat = (atirage->accel.dp_bkgd_clr >> 8) & 0xff;
                            else
                                src_dat = (atirage->accel.dp_bkgd_clr >> 16) & 0xff;
                        } else {
                            if (atirage->accel.xx_count == 2)
                                src_dat = (atirage->accel.dp_bkgd_clr >> 16) & 0xff;
                            else if (atirage->accel.xx_count == 1)
                                src_dat = (atirage->accel.dp_bkgd_clr >> 8) & 0xff;
                            else
                                src_dat = atirage->accel.dp_bkgd_clr & 0xff;
                        }
                    } else
                        src_dat = atirage->accel.dp_bkgd_clr;
                    break;
                case SRC_PAT:
                    if (atirage->pat_cntl & 2) {
                        src_dat = atirage->accel.pattern_clr4x2[dst_y & 1][dst_x & 3];
                        break;
                    } else if (atirage->pat_cntl & 4) {
                        src_dat = atirage->accel.pattern_clr8x1[dst_x & 7];
                        break;
                    }
                case SRC_SCALER_3D:
                    warning("Rectangle: Scaler / 3D source set");

                default:
                    src_dat = 0;
                    break;
            }

            if (atirage->dst_cntl & DST_POLYGON_EN) {
                int poly_src;
                READ(atirage->accel.src_offset + (src_y * atirage->accel.src_pitch) + src_x, poly_src, atirage->accel.src_size);
                if (poly_src)
                    atirage->accel.poly_draw = !atirage->accel.poly_draw;
            }

            if (!(atirage->dst_cntl & DST_POLYGON_EN) || atirage->accel.poly_draw) {
                READ(atirage->accel.dst_offset + ((dst_y) *atirage->accel.dst_pitch) + (dst_x), dest_dat, atirage->accel.dst_size);

                cmp_clr = atirage_blit_calc_cmp_clr(atirage, src_dat, dest_dat);

                if (!cmp_clr) {
                    old_dest_dat = dest_dat;
                    MIX
                    if (atirage->dst_cntl & DST_24_ROT_EN) {
                        if (atirage->accel.xinc == -1) {
                            if (atirage->accel.xx_count == 2)
                                write_mask = atirage->accel.write_mask & 0xff;
                            else if (atirage->accel.xx_count == 1)
                                write_mask = (atirage->accel.write_mask >> 8) & 0xff;
                            else
                                write_mask = (atirage->accel.write_mask >> 16) & 0xff;
                        } else {
                            if (atirage->accel.xx_count == 2)
                                write_mask = (atirage->accel.write_mask >> 16) & 0xff;
                            else if (atirage->accel.xx_count == 1)
                                write_mask = (atirage->accel.write_mask >> 8) & 0xff;
                            else
                                write_mask = atirage->accel.write_mask & 0xff;
                        }
                        dest_dat = (dest_dat & write_mask) | (old_dest_dat & ~write_mask);
                    } else
                        dest_dat = (dest_dat & atirage->accel.write_mask) | (old_dest_dat & ~atirage->accel.write_mask);
                }

                WRITE(atirage->accel.dst_offset + ((dst_y) * atirage->accel.dst_pitch) + (dst_x), atirage->accel.dst_size);
            }
        }

        atirage->accel.src_x += atirage->accel.xinc;
        atirage->accel.dst_x += atirage->accel.xinc;
        if (!(atirage->src_cntl & SRC_LINEAR_EN)) {
            atirage->accel.src_x_count--;
            if (atirage->accel.src_x_count <= 0) {
                atirage->accel.src_x = 0;
                if ((atirage->src_cntl & (SRC_PATT_ROT_EN | SRC_PATT_EN)) == (SRC_PATT_ROT_EN | SRC_PATT_EN)) {
                    atirage->accel.src_x_start = (atirage->src_y_x_start >> 16) & 0xfff;
                    if ((atirage->src_y_x_start >> 16) & 0x1000)
                        atirage->accel.src_x_start |= ~0xfff;
                    atirage->accel.src_x_count = atirage->accel.src_width2;
                } else
                    atirage->accel.src_x_count = atirage->accel.src_width1;
            }
        }

        atirage->accel.x_count--;
        atirage->accel.xx_count = (atirage->accel.xx_count + 1) % 3;
        if (atirage->accel.x_count <= 0) {
            atirage->accel.x_count  = atirage->accel.dst_width;
            atirage->accel.xx_count = 0;
            atirage->accel.dst_x    = 0;
            atirage->accel.dst_y += atirage->accel.yinc;
            atirage->accel.src_x_start = (atirage->src_y_x >> 16) & 0xfff;
            atirage->accel.src_x_count = atirage->accel.src_width1;

            if (!(atirage->src_cntl & SRC_LINEAR_EN)) {
                atirage->accel.src_x = 0;
                atirage->accel.src_y += atirage->accel.yinc;
                atirage->accel.src_y_count--;
                if (atirage->accel.src_y_count <= 0) {
                    atirage->accel.src_y = 0;
                    if ((atirage->src_cntl & (SRC_PATT_ROT_EN | SRC_PATT_EN)) == (SRC_PATT_ROT_EN | SRC_PATT_EN)) {
                        atirage->accel.src_y_start = atirage->src_y_x_start & 0x3fff;
                        if (atirage->src_y_x_start & 0x4000)
                            atirage->accel.src_y_start |= ~0x3fff;
                        atirage->accel.src_y_count = atirage->accel.src_height2;
                    } else
                        atirage->accel.src_y_count = atirage->accel.src_height1;
                }
            }

            atirage->accel.poly_draw = 0;
            atirage->accel.dst_height--;
            if (atirage->accel.dst_height <= 0) {
                /*Blit finished*/
                atirage_log("mach64 blit finished\n");
                atirage->accel.busy = 0;
                if (atirage->dst_cntl & DST_X_TILE)
                    atirage->dst_y_x = (atirage->dst_y_x & 0xfff) | ((atirage->dst_y_x + (atirage->accel.dst_width << 16)) & 0xfff0000);
                if (atirage->dst_cntl & DST_Y_TILE)
                    atirage->dst_y_x = (atirage->dst_y_x & 0xfff0000) | ((atirage->dst_y_x + (atirage->dst_height_width & 0x1fff)) & 0xfff);
                return;
            }
            if (atirage->host_cntl & HOST_BYTE_ALIGN) {
                if (atirage->accel.source_mix == MONO_SRC_HOST) {
                    if (atirage->dp_pix_width & DP_BYTE_PIX_ORDER)
                        cpu_dat >>= (count & 7);
                    else
                        cpu_dat <<= (count & 7);

                    count &= ~7;
                }
            }
        }
    }
}

void
atirage_blit_line(uint32_t cpu_dat, int count, atirage_t* atirage)
{
    svga_t *svga    = &atirage->svga;
    int     cmp_clr = 0;

    if (((atirage->crtc_gen_cntl >> 8) & 7) == BPP_24) {
        int x = 0;
        while (count) {
            uint32_t src_dat = 0;
            uint32_t dest_dat;
            uint32_t host_dat = 0;
            int      mix      = 0;
            int      draw_pixel = !(atirage->dst_cntl & DST_POLYGON_EN);

            if (atirage->dst_cntl & DST_POLYGON_EN) {
                if (atirage->dst_cntl & DST_Y_MAJOR)
                    draw_pixel = 1;
                else if (atirage->accel.err >= 0)
                    draw_pixel = 1;
            }

            if (atirage->accel.source_host) {
                host_dat = cpu_dat;

                if (atirage->accel.host_size < 2)
                    cpu_dat >>= (8 << atirage->accel.host_size); // shift by 8 for a word, 16 for a word, not at all for a dword.

                count -= (8 << atirage->accel.host_size);
            } else
                count--;

            switch (atirage->accel.source_mix) {
                case MONO_SRC_HOST:
                    if (atirage->dp_pix_width & DP_BYTE_PIX_ORDER) {
                        mix = cpu_dat & 1;
                        cpu_dat >>= 1;
                    } else {
                        mix = cpu_dat >> 31;
                        cpu_dat <<= 1;
                    }
                    break;
                case MONO_SRC_PAT:
                    mix = atirage->accel.pattern[atirage->accel.dst_y & 7][atirage->accel.dst_x & 7];
                    break;
                case MONO_SRC_1:
                    mix = 1;
                    break;
                case MONO_SRC_BLITSRC:
                    READ(atirage->accel.src_offset + (atirage->accel.src_y * atirage->accel.src_pitch) + atirage->accel.src_x, mix, WIDTH_1BIT);
                    break;
                default:
                    break;
            }

            if ((atirage->accel.dst_x >= atirage->accel.sc_left) && (atirage->accel.dst_x <= atirage->accel.sc_right) && (atirage->accel.dst_y >= atirage->accel.sc_top) && (atirage->accel.dst_y <= atirage->accel.sc_bottom) && draw_pixel) {
                switch (mix ? atirage->accel.source_fg : atirage->accel.source_bg) {
                    case SRC_HOST:
                        src_dat = host_dat;
                        break;
                    case SRC_BLITSRC:
                        if (atirage->accel.src_size == 0 && atirage->type == MACH64_GTB && (atirage->src_cntl & SRC_8x8x8_BRUSH))
                            src_dat = atirage->accel.pattern_clr8x8[atirage->accel.dst_y & 7][atirage->accel.dst_x & 7];
                        else
                            READ(atirage->accel.src_offset + (atirage->accel.src_y * atirage->accel.src_pitch) + atirage->accel.src_x, src_dat, atirage->accel.src_size);
                        break;
                    case SRC_FG:
                        src_dat = atirage->accel.dp_frgd_clr;
                        break;
                    case SRC_BG:
                        src_dat = atirage->accel.dp_bkgd_clr;
                        break;
                    case SRC_PAT:
                        if (atirage->pat_cntl & 2) {
                            src_dat = atirage->accel.pattern_clr4x2[atirage->accel.dst_y & 1][atirage->accel.dst_x & 3];
                            break;
                        } else if (atirage->pat_cntl & 4) {
                            src_dat = atirage->accel.pattern_clr8x1[atirage->accel.dst_x & 7];
                            break;
                        }
                    case SRC_SCALER_3D:
                        warning("Line: Scaler / 3D source set");
                        
                    default:
                        src_dat = 0;
                        break;
                }

                READ(atirage->accel.dst_offset + (atirage->accel.dst_y * atirage->accel.dst_pitch) + atirage->accel.dst_x, dest_dat, atirage->accel.dst_size);

                cmp_clr = atirage_blit_calc_cmp_clr(atirage, src_dat, dest_dat);

                if (!cmp_clr)
                    MIX

                if (!(atirage->dst_cntl & DST_Y_MAJOR)) {
                    if (!x)
                        dest_dat &= ~1;
                } else {
                    if (x == (atirage->accel.x_count - 1))
                        dest_dat &= ~1;
                }

                WRITE(atirage->accel.dst_offset + (atirage->accel.dst_y * atirage->accel.dst_pitch) + atirage->accel.dst_x, atirage->accel.dst_size);
            }

            x++;
            if (x >= atirage->accel.x_count) {
                atirage->accel.busy = 0;
                atirage_log("mach64 line24 finished\n");
                return;
            }

            if (atirage->dst_cntl & DST_Y_MAJOR) {
                atirage->accel.dst_y += atirage->accel.yinc;
                atirage->accel.src_y += atirage->accel.yinc;
                if (atirage->accel.err >= 0) {
                    atirage->accel.err += atirage->dst_bres_dec;
                    atirage->accel.dst_x += atirage->accel.xinc;
                    atirage->accel.src_x += atirage->accel.xinc;
                } else {
                    atirage->accel.err += atirage->dst_bres_inc;
                }
            } else {
                atirage->accel.dst_x += atirage->accel.xinc;
                atirage->accel.src_x += atirage->accel.xinc;
                if (atirage->accel.err >= 0) {
                    atirage->accel.err += atirage->dst_bres_dec;
                    atirage->accel.dst_y += atirage->accel.yinc;
                    atirage->accel.src_y += atirage->accel.yinc;
                } else {
                    atirage->accel.err += atirage->dst_bres_inc;
                }
            }
        }
    } else {
        while (count) {
            uint32_t src_dat = 0;
            uint32_t dest_dat;
            uint32_t host_dat   = 0;
            int      mix        = 0;
            int      draw_pixel = !(atirage->dst_cntl & DST_POLYGON_EN);

            if (atirage->accel.source_host) {
                host_dat = cpu_dat;
                if (atirage->accel.host_size < 2)
                    cpu_dat >>= (8 << atirage->accel.host_size); // shift by 8 for a word, 16 for a word, not at all for a dword.

                count -= (8 << atirage->accel.host_size);
            } else
                count--;

            switch (atirage->accel.source_mix) {
                case MONO_SRC_HOST:
                    mix = cpu_dat >> 31;
                    cpu_dat <<= 1;
                    break;
                case MONO_SRC_PAT:
                    mix = atirage->accel.pattern[atirage->accel.dst_y & 7][atirage->accel.dst_x & 7];
                    break;
                case MONO_SRC_1:
                default:
                    mix = 1;
                    break;
            }

            if (atirage->dst_cntl & DST_POLYGON_EN) {
                if (atirage->dst_cntl & DST_Y_MAJOR)
                    draw_pixel = 1;
                else if (atirage->accel.err >= 0)
                    draw_pixel = 1;
            }

            if (atirage->accel.x_count == 1 && !(atirage->dst_cntl & DST_LAST_PEL))
                draw_pixel = 0;

            if (atirage->accel.dst_x >= atirage->accel.sc_left && atirage->accel.dst_x <= atirage->accel.sc_right && atirage->accel.dst_y >= atirage->accel.sc_top && atirage->accel.dst_y <= atirage->accel.sc_bottom && draw_pixel) {
                switch (mix ? atirage->accel.source_fg : atirage->accel.source_bg) {
                    case SRC_HOST:
                        src_dat = host_dat;
                        break;
                    case SRC_BLITSRC:
                        READ(atirage->accel.src_offset + (atirage->accel.src_y * atirage->accel.src_pitch) + atirage->accel.src_x, src_dat, atirage->accel.src_size);
                        break;
                    case SRC_FG:
                        src_dat = atirage->accel.dp_frgd_clr;
                        break;
                    case SRC_BG:
                        src_dat = atirage->accel.dp_bkgd_clr;
                        break;
                    case SRC_SCALER_3D:
                        warning("Line: Scaler / 3D source set");
                    default:
                        src_dat = 0;
                        break;
                }

                READ(atirage->accel.dst_offset + (atirage->accel.dst_y * atirage->accel.dst_pitch) + atirage->accel.dst_x, dest_dat, atirage->accel.dst_size);

                cmp_clr = atirage_blit_calc_cmp_clr(atirage, src_dat, dest_dat);

                if (!cmp_clr)
                    MIX

                WRITE(atirage->accel.dst_offset + (atirage->accel.dst_y * atirage->accel.dst_pitch) + atirage->accel.dst_x, atirage->accel.dst_size);
            }

            atirage->accel.x_count--;
            if (atirage->accel.x_count <= 0) {
                /*Blit finished*/
                atirage_log("mach64 blit finished\n");
                atirage->accel.busy = 0;
                return;
            }

            if (atirage->dst_cntl & DST_Y_MAJOR) {
                atirage->accel.dst_y += atirage->accel.yinc;
                atirage->accel.src_y += atirage->accel.yinc;
                if (atirage->accel.err >= 0) {
                    atirage->accel.err += atirage->dst_bres_dec;
                    atirage->accel.dst_x += atirage->accel.xinc;
                    atirage->accel.src_x += atirage->accel.xinc;
                } else {
                    atirage->accel.err += atirage->dst_bres_inc;
                }
            } else {
                atirage->accel.dst_x += atirage->accel.xinc;
                atirage->accel.src_x += atirage->accel.xinc;
                if (atirage->accel.err >= 0) {
                    atirage->accel.err += atirage->dst_bres_dec;
                    atirage->accel.dst_y += atirage->accel.yinc;
                    atirage->accel.src_y += atirage->accel.yinc;
                } else {
                    atirage->accel.err += atirage->dst_bres_inc;
                }
            }
        }
    }
}

void
atirage_blit_trap(uint32_t cpu_dat, int count, atirage_t *atirage)
{
    svga_t *svga    = &atirage->svga;
    int cmp_clr = 0;
    if (((atirage->crtc_gen_cntl >> 8) & 7) == BPP_24) {
        warning("Trapezoid draw in 24bpp mode currently unsupported");

        int x = 0;
        while (count) {
            int      old_y       = atirage->accel.dst_y;
            int      span_done;
            int      decrement_count = 0;

            /* Todo: TEST ON REAL HARDWARE */
            if (atirage->dst_cntl & DST_POLYGON_EN) {
                warning("Attempted to draw trapezoid in polygon mode");
                atirage->accel.busy = 0;
                return;
            }
        
            
            if (atirage->dst_cntl & DST_Y_MAJOR) {







                atirage->accel.dst_y += atirage->accel.yinc;
                atirage->accel.src_y += atirage->accel.yinc;

                if ((atirage->accel.err > 0) || (atirage->accel.err == 0 && (((atirage->dst_cntl & BRES_SIGN_AUTO) & !(atirage->dst_cntl & DST_X_DIR)) | !(atirage->dst_cntl & (BRES_SIGN_AUTO | DST_BRES_SIGN))))) {
                    atirage->accel.err += atirage->dst_bres_dec;
                    atirage->accel.dst_x += atirage->accel.xinc;
                } else {
                    atirage->accel.err += atirage->dst_bres_inc;
                }
                
                if ((atirage->accel.trail_err > 0) || (atirage->accel.trail_err == 0 && (((atirage->dst_cntl & BRES_SIGN_AUTO) & !(atirage->dst_cntl & TRAIL_X_DIR)) | !(atirage->dst_cntl & (BRES_SIGN_AUTO | DST_BRES_SIGN))))) {
                    atirage->accel.trail_err += atirage->trail_bres_dec;
                    atirage->accel.trail_x += atirage->accel.trail_xinc; 
                } else {
                    atirage->accel.trail_err += atirage->trail_bres_inc;
                }
            } else {
                if (!atirage->accel.y_count && atirage->accel.temp_cnt < 0) {
                    atirage->accel.dst_x += atirage->accel.xinc;
                    atirage->accel.src_x += atirage->accel.xinc;
                    if ((atirage->accel.err > 0) || (atirage->accel.err == 0 && (((atirage->dst_cntl & BRES_SIGN_AUTO) & !(atirage->dst_cntl & DST_Y_DIR)) | !(atirage->dst_cntl & (BRES_SIGN_AUTO | DST_BRES_SIGN))))) {
                        atirage->accel.err += atirage->dst_bres_dec;
                        atirage->accel.dst_y += atirage->accel.yinc;
                    } else {
                        atirage->accel.err += atirage->dst_bres_inc;
                    }

                    atirage->accel.trail_x += atirage->accel.trail_xinc; 

                    if ((atirage->accel.trail_err > 0) || (atirage->accel.trail_err == 0 && (((atirage->dst_cntl & BRES_SIGN_AUTO) & !(atirage->dst_cntl & DST_Y_DIR)) | !(atirage->dst_cntl & (BRES_SIGN_AUTO | DST_BRES_SIGN))))) {
                        atirage->accel.trail_err += atirage->trail_bres_dec;
                    } else {
                        atirage->accel.trail_err += atirage->trail_bres_inc;
                    }
                }











            }
        
            x++;
            if (x >= atirage->accel.x_count) {
                atirage->accel.busy = 0;
                atirage_log("mach64 trapezoid 24bpp finished\n");
                return;
            }
        
        
        
        }




        atirage->accel.busy = 0;
    } else {
        while (count) {
            int      old_y       = atirage->accel.dst_y;
            int      span_done;
            int      decrement_count = 0;

            /* Todo: TEST ON REAL HARDWARE */
            if (atirage->dst_cntl & DST_POLYGON_EN) {
                warning("Attempted to draw trapezoid in polygon mode");
                atirage->accel.busy = 0;
                return;
            }

            if (atirage->dst_cntl & DST_Y_MAJOR) { 
                span_done = atirage_write_trap(atirage, &cpu_dat, &count);
                if (!span_done)
                    return;

                decrement_count = 1;

                atirage->accel.dst_y += atirage->accel.yinc;
                atirage->accel.src_y += atirage->accel.yinc;

                if ((atirage->accel.err > 0) || (atirage->accel.err == 0 && (((atirage->dst_cntl & BRES_SIGN_AUTO) & !(atirage->dst_cntl & DST_X_DIR)) | !(atirage->dst_cntl & (BRES_SIGN_AUTO | DST_BRES_SIGN))))) {
                    atirage->accel.err += atirage->dst_bres_dec;
                    atirage->accel.dst_x += atirage->accel.xinc;
                } else {
                    atirage->accel.err += atirage->dst_bres_inc;
                }
                
                if ((atirage->accel.trail_err > 0) || (atirage->accel.trail_err == 0 && (((atirage->dst_cntl & BRES_SIGN_AUTO) & !(atirage->dst_cntl & TRAIL_X_DIR)) | !(atirage->dst_cntl & (BRES_SIGN_AUTO | DST_BRES_SIGN))))) {
                    atirage->accel.trail_err += atirage->trail_bres_dec;
                    atirage->accel.trail_x += atirage->accel.trail_xinc; 
                } else {
                    atirage->accel.trail_err += atirage->trail_bres_inc;
                }

            } else {
                if (!atirage->accel.y_count && atirage->accel.temp_cnt < 0) {
                    atirage->accel.dst_x += atirage->accel.xinc;
                    atirage->accel.src_x += atirage->accel.xinc;
                    if ((atirage->accel.err > 0) || (atirage->accel.err == 0 && (((atirage->dst_cntl & BRES_SIGN_AUTO) & !(atirage->dst_cntl & DST_Y_DIR)) | !(atirage->dst_cntl & (BRES_SIGN_AUTO | DST_BRES_SIGN))))) {
                        atirage->accel.err += atirage->dst_bres_dec;
                        atirage->accel.dst_y += atirage->accel.yinc;
                    } else {
                        atirage->accel.err += atirage->dst_bres_inc;
                    }

                    atirage->accel.trail_x += atirage->accel.trail_xinc; 

                    if ((atirage->accel.trail_err > 0) || (atirage->accel.trail_err == 0 && (((atirage->dst_cntl & BRES_SIGN_AUTO) & !(atirage->dst_cntl & DST_Y_DIR)) | !(atirage->dst_cntl & (BRES_SIGN_AUTO | DST_BRES_SIGN))))) {
                        atirage->accel.trail_err += atirage->trail_bres_dec;
                    } else {
                        atirage->accel.trail_err += atirage->trail_bres_inc;
                    }
                }
                
                if ((atirage->accel.dst_y != old_y) || atirage->accel.y_count) {
                    span_done = atirage_write_trap(atirage, &cpu_dat, &count);
                    if (!span_done)
                        return;

                    atirage->accel.src_y += atirage->accel.yinc;
                    decrement_count = 1;
                }
            }

            /* Todo: SRC_LINEAR_EN */

            if (decrement_count)
                atirage->accel.x_count--;
            if (atirage->accel.x_count <= 0) {
                /*Blit finished*/
                atirage_log("mach64 trapezoid finished\n");
                atirage->accel.busy = 0;
                return;
            }
            if (atirage->accel.y_count)
                atirage->accel.y_count = 0;
        }
    }
}

static int
atirage_write_trap(atirage_t *atirage, uint32_t *cpu_dat, int *count)
{
    svga_t *svga    = &atirage->svga;
    int cmp_clr = 0;

    int span_left   = ((atirage->accel.dst_x) > (atirage->accel.trail_x)) ? (atirage->accel.trail_x) : (atirage->accel.dst_x);
    int span_right  = ((atirage->accel.dst_x) <= (atirage->accel.trail_x)) ? (atirage->accel.trail_x) : (atirage->accel.dst_x);

    int draw_left   = (span_left < atirage->accel.sc_left) ? atirage->accel.sc_left : span_left;
    int draw_right  = (span_right > atirage->accel.sc_right) ? atirage->accel.sc_right : span_right;

    int mix = 0;
    int span_inc = (atirage->dst_cntl & TRAP_FILL_DIR) ? 1 : -1;


    uint32_t src_dat = 0;
    uint32_t dest_dat;
    uint32_t host_dat;

    if (atirage->accel.temp_cnt < 0)
        atirage->accel.temp_cnt = (atirage->dst_cntl & TRAP_FILL_DIR) ? draw_left : draw_right;

    if (draw_left <= draw_right &&
        atirage->accel.dst_y >= atirage->accel.sc_top &&
        atirage->accel.dst_y <= atirage->accel.sc_bottom) {
        
        if (atirage->trapezoid_debug) {
            warning("dst_pitch=%d",atirage->accel.dst_pitch);
            warning("dst_y=%d, dst_x=%d, trail_x=%d\n", atirage->accel.dst_y, atirage->accel.dst_x, atirage->accel.trail_x);
        }

        if (atirage->dst_cntl & TRAP_FILL_DIR) {
            for (int s = atirage->accel.temp_cnt; s <= draw_right; s++) {
                if (*count <= 0 && atirage->accel.source_host) { /* Count is -1 if non-host */
                    atirage->accel.temp_cnt = s;
                    return 0;
                }
                
                if (atirage->accel.source_host) {
                    host_dat = *cpu_dat;
                    if (atirage->accel.host_size < 2)
                        *cpu_dat >>= (8 << atirage->accel.host_size); // shift by 8 for a word, 16 for a word, not at all for a dword.
                    *count -= (8 << atirage->accel.host_size);
                } else {
                    (*count)--;
                }

                if (((atirage->crtc_gen_cntl >> 8) & 7) == BPP_24) {
                    switch (atirage->accel.source_mix) {
                        case MONO_SRC_HOST:
                            if (atirage->dp_pix_width & DP_BYTE_PIX_ORDER) {
                                mix = *cpu_dat & 1;
                                *cpu_dat >>= 1;
                            } else {
                                mix = *cpu_dat >> 31;
                                *cpu_dat <<= 1;
                            }
                            break;
                        case MONO_SRC_PAT:
                            mix = atirage->accel.pattern[atirage->accel.dst_y & 7][s & 7];
                            break;
                        case MONO_SRC_1:
                            mix = 1;
                            break;
                        case MONO_SRC_BLITSRC:
                            READ(atirage->accel.src_offset + (atirage->accel.src_y * atirage->accel.src_pitch) + atirage->accel.src_x, mix, WIDTH_1BIT);
                            break;
                        default:
                            break;
                    }
                } else {
                    switch (atirage->accel.source_mix) {
                        case MONO_SRC_HOST:
                            mix = *cpu_dat >> 31;
                            *cpu_dat <<= 1;
                            break;
                        case MONO_SRC_PAT:
                            mix = atirage->accel.pattern[atirage->accel.dst_y & 7][s & 7];
                            break;
                        case MONO_SRC_1:
                        default:
                            mix = 1;
                            break;
                    }
                }

                switch (mix ? atirage->accel.source_fg : atirage->accel.source_bg) {
                    case SRC_HOST:
                        src_dat = host_dat;
                        break;
                    case SRC_BLITSRC:
                        READ(atirage->accel.src_offset + (atirage->accel.src_y * atirage->accel.src_pitch) + atirage->accel.src_x, src_dat, atirage->accel.src_size);
                        break;
                    case SRC_FG:
                        src_dat = atirage->accel.dp_frgd_clr;
                        break;
                    case SRC_BG:
                        src_dat = atirage->accel.dp_bkgd_clr;
                        break;
                    case SRC_SCALER_3D:
                        warning("Trapezoid: Scaler / 3D source set");
                    default:
                        src_dat = 0;
                        break;
                }
                if (((atirage->crtc_gen_cntl >> 8) & 7) == BPP_24) {
                    /* According to the Rage Pro programmer's guide line drawing is unsupported in 24bpp mode, might implement later */
                } else {
                    READ(atirage->accel.dst_offset + (atirage->accel.dst_y * atirage->accel.dst_pitch) + s, dest_dat, atirage->accel.dst_size);
                    cmp_clr = atirage_blit_calc_cmp_clr(atirage, src_dat, dest_dat);
                    if (!cmp_clr)
                        MIX
                    WRITE(atirage->accel.dst_offset + (atirage->accel.dst_y * atirage->accel.dst_pitch) + s, atirage->accel.dst_size);
                }
            }
        } else {
            for (int s = atirage->accel.temp_cnt; s >= draw_left; s--) {
                if (*count <= 0 && atirage->accel.source_host) { /* Count is -1 if non-host */
                    atirage->accel.temp_cnt = s;
                    return 0;
                }
                
                if (atirage->accel.source_host) {
                    host_dat = *cpu_dat;
                    if (atirage->accel.host_size < 2)
                        *cpu_dat >>= (8 << atirage->accel.host_size); // shift by 8 for a word, 16 for a word, not at all for a dword.
                    *count -= (8 << atirage->accel.host_size);
                } else {
                    (*count)--;
                }

                switch (atirage->accel.source_mix) {
                    case MONO_SRC_HOST:
                        mix = *cpu_dat >> 31;
                        *cpu_dat <<= 1;
                        break;
                    case MONO_SRC_PAT:
                        mix = atirage->accel.pattern[atirage->accel.dst_y & 7][s & 7];
                        break;
                    case MONO_SRC_1:
                    default:
                        mix = 1;
                        break;
                }
                
                switch (mix ? atirage->accel.source_fg : atirage->accel.source_bg) {
                    case SRC_HOST:
                        src_dat = host_dat;
                        break;
                    case SRC_BLITSRC:
                        READ(atirage->accel.src_offset + (atirage->accel.src_y * atirage->accel.src_pitch) + atirage->accel.src_x, src_dat, atirage->accel.src_size);
                        break;
                    case SRC_FG:
                        src_dat = atirage->accel.dp_frgd_clr;
                        break;
                    case SRC_BG:
                        src_dat = atirage->accel.dp_bkgd_clr;
                        break;
                    case SRC_SCALER_3D:
                        warning("Trapezoid: Scaler / 3D source set");
                    default:
                        src_dat = 0;
                        break;
                }
                if (((atirage->crtc_gen_cntl >> 8) & 7) == BPP_24) {
                    /* According to the Rage Pro programmer's guide line drawing is unsupported in 24bpp mode, might implement later */
                } else {
                    READ(atirage->accel.dst_offset + (atirage->accel.dst_y * atirage->accel.dst_pitch) + s, dest_dat, atirage->accel.dst_size);
                    cmp_clr = atirage_blit_calc_cmp_clr(atirage, src_dat, dest_dat);
                    if (!cmp_clr)
                        MIX
                    WRITE(atirage->accel.dst_offset + (atirage->accel.dst_y * atirage->accel.dst_pitch) + s, atirage->accel.dst_size);
                }
            }
        }
        atirage->accel.temp_cnt = -1;
        return 1;
    } else {
        atirage->accel.temp_cnt = -1;
        return 1;
    }
}




void
atirage_blit(uint32_t cpu_dat, int count, atirage_t *atirage)
{
    if (!atirage->accel.busy) {
        atirage_log("atirage_blit : return as not busy\n");
        return;
    }

    switch (atirage->accel.op) {
        case OP_RECT:
            atirage_blit_rect(cpu_dat, count, atirage);
            break;
        case OP_LINE:
            atirage_blit_line(cpu_dat, count, atirage);
            break;
        case OP_TRAP:
            atirage_blit_trap(cpu_dat, count, atirage);
        default:
            break;
    }
}

void
atirage_load_context(atirage_t *atirage)
{
    svga_t  *svga = &atirage->svga;
    uint32_t addr;

    while (atirage->context_load_cntl & 0x30000) {
        addr                 = ((0x3fff - (atirage->context_load_cntl & 0x3fff)) * 256) & atirage->vram_mask;
        atirage->context_mask = *(uint32_t *) &svga->vram[addr];
        atirage_log("atirage_load_context %08X from %08X : mask %08X\n", atirage->context_load_cntl, addr, atirage->context_mask);

        if (atirage->context_mask & (1 << 2))
            atirage_accel_write_fifo_l(atirage, 0x100, *(uint32_t *) &svga->vram[addr + 0x08]);
        if (atirage->context_mask & (1 << 3))
            atirage_accel_write_fifo_l(atirage, 0x10c, *(uint32_t *) &svga->vram[addr + 0x0c]);
        if (atirage->context_mask & (1 << 4))
            atirage_accel_write_fifo_l(atirage, 0x118, *(uint32_t *) &svga->vram[addr + 0x10]);
        if (atirage->context_mask & (1 << 5))
            atirage_accel_write_fifo_l(atirage, 0x124, *(uint32_t *) &svga->vram[addr + 0x14]);
        if (atirage->context_mask & (1 << 6))
            atirage_accel_write_fifo_l(atirage, 0x128, *(uint32_t *) &svga->vram[addr + 0x18]);
        if (atirage->context_mask & (1 << 7))
            atirage_accel_write_fifo_l(atirage, 0x12c, *(uint32_t *) &svga->vram[addr + 0x1c]);
        if (atirage->context_mask & (1 << 8))
            atirage_accel_write_fifo_l(atirage, 0x180, *(uint32_t *) &svga->vram[addr + 0x20]);
        if (atirage->context_mask & (1 << 9))
            atirage_accel_write_fifo_l(atirage, 0x18c, *(uint32_t *) &svga->vram[addr + 0x24]);
        if (atirage->context_mask & (1 << 10))
            atirage_accel_write_fifo_l(atirage, 0x198, *(uint32_t *) &svga->vram[addr + 0x28]);
        if (atirage->context_mask & (1 << 11))
            atirage_accel_write_fifo_l(atirage, 0x1a4, *(uint32_t *) &svga->vram[addr + 0x2c]);
        if (atirage->context_mask & (1 << 12))
            atirage_accel_write_fifo_l(atirage, 0x1b0, *(uint32_t *) &svga->vram[addr + 0x30]);
        if (atirage->context_mask & (1 << 13))
            atirage_accel_write_fifo_l(atirage, 0x280, *(uint32_t *) &svga->vram[addr + 0x34]);
        if (atirage->context_mask & (1 << 14))
            atirage_accel_write_fifo_l(atirage, 0x284, *(uint32_t *) &svga->vram[addr + 0x38]);
        if (atirage->context_mask & (1 << 15))
            atirage_accel_write_fifo_l(atirage, 0x2a8, *(uint32_t *) &svga->vram[addr + 0x3c]);
        if (atirage->context_mask & (1 << 16))
            atirage_accel_write_fifo_l(atirage, 0x2b4, *(uint32_t *) &svga->vram[addr + 0x40]);
        if (atirage->context_mask & (1 << 17))
            atirage_accel_write_fifo_l(atirage, 0x2c0, *(uint32_t *) &svga->vram[addr + 0x44]);
        if (atirage->context_mask & (1 << 18))
            atirage_accel_write_fifo_l(atirage, 0x2c4, *(uint32_t *) &svga->vram[addr + 0x48]);
        if (atirage->context_mask & (1 << 19))
            atirage_accel_write_fifo_l(atirage, 0x2c8, *(uint32_t *) &svga->vram[addr + 0x4c]);
        if (atirage->context_mask & (1 << 20))
            atirage_accel_write_fifo_l(atirage, 0x2cc, *(uint32_t *) &svga->vram[addr + 0x50]);
        if (atirage->context_mask & (1 << 21))
            atirage_accel_write_fifo_l(atirage, 0x2d0, *(uint32_t *) &svga->vram[addr + 0x54]);
        if (atirage->context_mask & (1 << 22))
            atirage_accel_write_fifo_l(atirage, 0x2d4, *(uint32_t *) &svga->vram[addr + 0x58]);
        if (atirage->context_mask & (1 << 23))
            atirage_accel_write_fifo_l(atirage, 0x2d8, *(uint32_t *) &svga->vram[addr + 0x5c]);
        if (atirage->context_mask & (1 << 24))
            atirage_accel_write_fifo_l(atirage, 0x300, *(uint32_t *) &svga->vram[addr + 0x60]);
        if (atirage->context_mask & (1 << 25))
            atirage_accel_write_fifo_l(atirage, 0x304, *(uint32_t *) &svga->vram[addr + 0x64]);
        if (atirage->context_mask & (1 << 26))
            atirage_accel_write_fifo_l(atirage, 0x308, *(uint32_t *) &svga->vram[addr + 0x68]);
        if (atirage->context_mask & (1 << 27))
            atirage_accel_write_fifo_l(atirage, 0x330, *(uint32_t *) &svga->vram[addr + 0x6c]);

        atirage->context_load_cntl = *(uint32_t *) &svga->vram[addr + 0x70];
    }
}


//
// Overlay
//

#define CLAMP(x)                      \
    do {                              \
        if ((x) & ~0xff)              \
            x = ((x) < 0) ? 0 : 0xff; \
    } while (0)

#define DECODE_ARGB1555()                                            \
    do {                                                             \
        for (x = 0; x < atirage->svga.overlay_latch.cur_xsize; x++) { \
            uint16_t dat = ((uint16_t *) src)[x];                    \
                                                                     \
            int b = dat & 0x1f;                                      \
            int g = (dat >> 5) & 0x1f;                               \
            int r = (dat >> 10) & 0x1f;                              \
                                                                     \
            b = (b << 3) | (b >> 2);                                 \
            g = (g << 3) | (g >> 2);                                 \
            r = (r << 3) | (r >> 2);                                 \
                                                                     \
            atirage->overlay_dat[x] = (r << 16) | (g << 8) | b;       \
        }                                                            \
    } while (0)

#define DECODE_RGB565()                                              \
    do {                                                             \
        for (x = 0; x < atirage->svga.overlay_latch.cur_xsize; x++) { \
            uint16_t dat = ((uint16_t *) src)[x];                    \
                                                                     \
            int b = dat & 0x1f;                                      \
            int g = (dat >> 5) & 0x3f;                               \
            int r = (dat >> 11) & 0x1f;                              \
                                                                     \
            b = (b << 3) | (b >> 2);                                 \
            g = (g << 2) | (g >> 4);                                 \
            r = (r << 3) | (r >> 2);                                 \
                                                                     \
            atirage->overlay_dat[x] = (r << 16) | (g << 8) | b;       \
        }                                                            \
    } while (0)

#define DECODE_ARGB8888()                                            \
    do {                                                             \
        for (x = 0; x < atirage->svga.overlay_latch.cur_xsize; x++) { \
            int b = src[0];                                          \
            int g = src[1];                                          \
            int r = src[2];                                          \
            src += 4;                                                \
                                                                     \
            atirage->overlay_dat[x] = (r << 16) | (g << 8) | b;       \
        }                                                            \
    } while (0)

#define DECODE_VYUY422()                                                 \
    do {                                                                 \
        for (x = 0; x < src_w; x += 1) {                                 \
            uint8_t y1, y2;                                              \
            int8_t  u, v;                                                \
            int     dR, dG, dB;                                          \
            int     r, g, b;                                             \
                                                                         \
            y1 = src[0];                                                 \
            u  = src[1] - 0x80;                                          \
            y2 = src[2];                                                 \
            v  = src[3] - 0x80;                                          \
            src += 4;                                                    \
                                                                         \
            dR = (359 * v) >> 8;                                         \
            dG = (88 * u + 183 * v) >> 8;                                \
            dB = (453 * u) >> 8;                                         \
                                                                         \
            r = y1 + dR;                                                 \
            CLAMP(r);                                                    \
            g = y1 - dG;                                                 \
            CLAMP(g);                                                    \
            b = y1 + dB;                                                 \
            CLAMP(b);                                                    \
            atirage->overlay_dat[x * 2] = (r << 16) | (g << 8) | b;       \
                                                                         \
            r = y2 + dR;                                                 \
            CLAMP(r);                                                    \
            g = y2 - dG;                                                 \
            CLAMP(g);                                                    \
            b = y2 + dB;                                                 \
            CLAMP(b);                                                    \
            atirage->overlay_dat[(x * 2) + 1] = (r << 16) | (g << 8) | b; \
        }                                                                \
    } while (0)

#define DECODE_YVYU422()                                                 \
    do {                                                                 \
        for (x = 0; x < src_w; x += 1) {                                 \
            uint8_t y1, y2;                                              \
            int8_t  u, v;                                                \
            int     dR, dG, dB;                                          \
            int     r, g, b;                                             \
                                                                         \
            u  = src[0] - 0x80;                                          \
            y1 = src[1];                                                 \
            v  = src[2] - 0x80;                                          \
            y2 = src[3];                                                 \
            src += 4;                                                    \
                                                                         \
            dR = (359 * v) >> 8;                                         \
            dG = (88 * u + 183 * v) >> 8;                                \
            dB = (453 * u) >> 8;                                         \
                                                                         \
            r = y1 + dR;                                                 \
            CLAMP(r);                                                    \
            g = y1 - dG;                                                 \
            CLAMP(g);                                                    \
            b = y1 + dB;                                                 \
            CLAMP(b);                                                    \
            atirage->overlay_dat[x * 2] = (r << 16) | (g << 8) | b;       \
                                                                         \
            r = y2 + dR;                                                 \
            CLAMP(r);                                                    \
            g = y2 - dG;                                                 \
            CLAMP(g);                                                    \
            b = y2 + dB;                                                 \
            CLAMP(b);                                                    \
            atirage->overlay_dat[(x * 2) + 1] = (r << 16) | (g << 8) | b; \
        }                                                                \
    } while (0)

#define DECODE_YUV12_PACKED()                                            \
    do {                                                                 \
        for (x = 0; x < src_w; x += 1) {                                 \
            uint8_t y1, y2;                                              \
            int8_t  u, v;                                                \
            int     dR, dG, dB;                                          \
            int     r, g, b;                                             \
                                                                         \
            u  = uvsrc[3] - 0x80;                                        \
            y1 = src[0];                                                 \
            v  = uvsrc[2] - 0x80;                                        \
            y2 = src[1];                                                 \
            src += 4;                                                    \
            uvsrc += 4;                                                  \
                                                                         \
            dR = (359 * v) >> 8;                                         \
            dG = (88 * u + 183 * v) >> 8;                                \
            dB = (453 * u) >> 8;                                         \
                                                                         \
            r = y1 + dR;                                                 \
            CLAMP(r);                                                    \
            g = y1 - dG;                                                 \
            CLAMP(g);                                                    \
            b = y1 + dB;                                                 \
            CLAMP(b);                                                    \
            atirage->overlay_dat[x * 2] = (r << 16) | (g << 8) | b;       \
                                                                         \
            r = y2 + dR;                                                 \
            CLAMP(r);                                                    \
            g = y2 - dG;                                                 \
            CLAMP(g);                                                    \
            b = y2 + dB;                                                 \
            CLAMP(b);                                                    \
            atirage->overlay_dat[(x * 2) + 1] = (r << 16) | (g << 8) | b; \
        }                                                                \
    } while (0)

void
atirage_overlay_draw(svga_t *svga, int displine)
{
    atirage_t *atirage = (atirage_t *) svga->priv;
    int       x;
    int       h_acc = 0;
    int       h_max = (atirage->scaler_height_width >> 16) & 0x3ff;
    int       src_w = h_max;
    int       h_inc = atirage->overlay_scale_inc >> 16;
    int       v_max = atirage->scaler_height_width & 0x3ff;
    int       v_inc = atirage->overlay_scale_inc & 0xffff;
    uint32_t *p;
    uint8_t  *src   = &svga->vram[svga->overlay.addr];
    uint8_t  *uvsrc = src;
    int       old_y = atirage->overlay_v_acc;
    int       y_diff;
    int       video_key_fn    = atirage->overlay_key_cntl & 5;
    int       graphics_key_fn = (atirage->overlay_key_cntl >> 4) & 5;
    int       overlay_cmp_mix = (atirage->overlay_key_cntl >> 8) & 0xf;
    int       gfx_src         = 0;
    int       desktop_x = atirage->svga.overlay_latch.x;
    int       desktop_y = displine - svga->y_add;

    p = &buffer32->line[displine][svga->x_add + atirage->svga.overlay_latch.x];

    if (atirage->overlay_cur_y >= 2) {
        /* Avoid corrupt UV data on YUV12 packed modes */
        uvsrc = &svga->vram[atirage->overlay_base + svga->overlay.pitch * 2 * (!(atirage->overlay_cur_y & 1) ? (atirage->overlay_cur_y + 1) : atirage->overlay_cur_y)];
    }

    if (atirage->scaler_update) {
        switch (atirage->scaler_format) {
            case 0x3:
                DECODE_ARGB1555();
                break;
            case 0x4:
                DECODE_RGB565();
                break;
            case 0x6:
                DECODE_ARGB8888();
                break;
            case 0xa:
                DECODE_YUV12_PACKED();
                break;
            case 0xb:
                DECODE_VYUY422();
                break;
            case 0xc:
                DECODE_YVYU422();
                break;
            default:
                pclog("Unknown Mach64 scaler format %x\n", atirage->scaler_format);
                /*Fill buffer with something recognisably wrong*/
                for (x = 0; x < atirage->svga.overlay_latch.cur_xsize; x++)
                    atirage->overlay_dat[x] = 0xff00ff;
                break;
        }
    }

    if (overlay_cmp_mix == 2) {
        for (x = 0; x < atirage->svga.overlay_latch.cur_xsize; x++) {
            int h = h_acc >> 12;

            p[x] = atirage->overlay_dat[h];

            h_acc += h_inc;
            if (h_acc > (h_max << 12))
                h_acc = (h_max << 12);
        }
    } else {
        for (x = 0; x < atirage->svga.overlay_latch.cur_xsize; x++) {
            int h         = h_acc >> 12;
            int gr_cmp    = 0;
            int vid_cmp   = 0;
            int use_video = 0;

            switch (video_key_fn) {
                case 0:
                    vid_cmp = 0;
                    break;
                case 1:
                    vid_cmp = 1;
                    break;
                case 4:
                    vid_cmp = ((atirage->overlay_dat[h] ^ atirage->overlay_video_key_clr) & atirage->overlay_video_key_msk);
                    break;
                case 5:
                    vid_cmp = !((atirage->overlay_dat[h] ^ atirage->overlay_video_key_clr) & atirage->overlay_video_key_msk);
                    break;
                default:
                    break;
            }
            switch (svga->bpp) {
                case 8:
                    gfx_src = svga->vram[desktop_y * (svga->rowoffset * 8) + (desktop_x + x) * 1 + svga->memaddr_latch * 4];
                    break;
                case 15:
                case 16:
                    gfx_src = *(uint16_t*)&svga->vram[desktop_y * (svga->rowoffset * 8) + (desktop_x + x) * 2 + svga->memaddr_latch * 4];
                    break;
                case 24:
                    gfx_src = svga->vram[desktop_y * (svga->rowoffset * 8) + (desktop_x + x) * 3 + svga->memaddr_latch * 4]
                            | (svga->vram[desktop_y * (svga->rowoffset * 8) + (desktop_x + x) * 3 + 1 + svga->memaddr_latch * 4] << 8)
                            | (svga->vram[desktop_y * (svga->rowoffset * 8) + (desktop_x + x) * 3 + 2 + svga->memaddr_latch * 4] << 16);
                    break;
                case 32:
                    gfx_src = *(uint32_t*)&svga->vram[desktop_y * (svga->rowoffset * 8) + (desktop_x + x) * 4 + svga->memaddr_latch * 4];
                    break;
            }
            switch (graphics_key_fn) {
                case 0:
                    gr_cmp = 0;
                    break;
                case 1:
                    gr_cmp = 1;
                    break;
                case 4:
                    gr_cmp = ((gfx_src ^ atirage->overlay_graphics_key_clr) & atirage->overlay_graphics_key_msk & 0xffffff);
                    break;
                case 5:
                    gr_cmp = !((gfx_src ^ atirage->overlay_graphics_key_clr) & atirage->overlay_graphics_key_msk & 0xffffff);
                    break;
                default:
                    break;
            }
            vid_cmp = vid_cmp ?-1 : 0;
            gr_cmp  = gr_cmp ? -1 : 0;

            switch (overlay_cmp_mix) {
                case 0x0:
                    use_video = gr_cmp;
                    break;
                case 0x1:
                    use_video = 0;
                    break;
                case 0x2:
                    use_video = ~0;
                    break;
                case 0x3:
                    use_video = ~gr_cmp;
                    break;
                case 0x4:
                    use_video = ~vid_cmp;
                    break;
                case 0x5:
                    use_video = gr_cmp ^ vid_cmp;
                    break;
                case 0x6:
                    use_video = ~gr_cmp ^ vid_cmp;
                    break;
                case 0x7:
                    use_video = vid_cmp;
                    break;
                case 0x8:
                    use_video = ~gr_cmp | ~vid_cmp;
                    break;
                case 0x9:
                    use_video = gr_cmp | ~vid_cmp;
                    break;
                case 0xa:
                    use_video = ~gr_cmp | vid_cmp;
                    break;
                case 0xb:
                    use_video = gr_cmp | vid_cmp;
                    break;
                case 0xc:
                    use_video = gr_cmp & vid_cmp;
                    break;
                case 0xd:
                    use_video = ~gr_cmp & vid_cmp;
                    break;
                case 0xe:
                    use_video = gr_cmp & ~vid_cmp;
                    break;
                case 0xf:
                    use_video = ~gr_cmp & ~vid_cmp;
                    break;
                default:
                    break;
            }

            if (use_video)
                p[x] = atirage->overlay_dat[h];

            h_acc += h_inc;
            if (h_acc > (h_max << 12))
                h_acc = (h_max << 12);
        }
    }

    atirage->overlay_v_acc += v_inc;
    if (atirage->overlay_v_acc > (v_max << 12))
        atirage->overlay_v_acc = v_max << 12;

    y_diff = (atirage->overlay_v_acc >> 12) - (old_y >> 12);

    if (atirage->scaler_format == 6)
        svga->overlay.addr += svga->overlay.pitch * 4 * y_diff;
    else
        svga->overlay.addr += svga->overlay.pitch * 2 * y_diff;

    atirage->scaler_update = y_diff;
    atirage->overlay_cur_y += y_diff;
}
