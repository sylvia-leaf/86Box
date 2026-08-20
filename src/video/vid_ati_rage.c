/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          ATi Mach64 graphics card emulation.
 *          The ATi Mach64 is a 1994 Windows 2D accelerator.
 *          Technical information is available at: https://bitsavers.org/components/ati/RRG-S00700-05_mach64_Register_Reference_Guide_1999410.pdf
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

video_timings_t timing_atirage = { .type = VIDEO_PCI, .write_b = 2, .write_w = 2, .write_l = 1, .read_b = 20, .read_w = 20, .read_l = 21 };

atirage_t *reset_state[2] = { NULL, NULL };

int atirage_width[8] = { WIDTH_1BIT, 0, 0, 1, 1, 2, 2, 0 };

#ifdef ENABLE_ATIRAGE_LOG
int atirage_do_log = ENABLE_ATIRAGE_LOG;

void
atirage_log(const char *fmt, ...);
{
    va_list ap;

    if (atirage_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#endif 

#define USE_RAGE_BUS_CNTL

// x86 I/O port output function
void
atirage_out(uint16_t addr, uint8_t val, void *priv)
{
    atirage_t *atirage = priv;
    svga_t   *svga   = &atirage->svga;
    uint8_t   old;

    // pclog( "RAGE out addr=0x%03X value=%04X\n", addr, val);

    if (((addr & 0xFFF0) == 0x3D0 || (addr & 0xFFF0) == 0x3B0) && !(svga->miscout & 1))
        addr ^= 0x60;

    switch (addr) {
        case 0x1ce:
            atirage->index = val;
            break;
        case 0x1cf:
            atirage->regs[atirage->index & 0x3f] = val;
            if ((atirage->index & 0x3f) == 0x36)
                svga_recalctimings(svga);
            break;
        case 0x3C6 ... 0x3C9:
            svga_out(addr, val, svga);
            return;
        case 0x3cf:
            // pclog( "VGA GDC6 out addr=0x3cf value=%04X\n", val);
            if (svga->gdcaddr == 6) {
                uint8_t old_val = svga->gdcreg[6];
                svga->gdcreg[6] = val;
                if ((svga->gdcreg[6] & 0xc) != (old_val & 0xc))
                    atirage_updatemapping(atirage);
                return;
            }
            break;
        case 0x3D4:
            svga->crtcreg = val & 0x3f;
            return;
        case 0x3D5:
            if (svga->crtcreg > 0x20)
                return;
            if ((svga->crtcreg < 7) && (svga->crtc[0x11] & 0x80))
                return;
            if ((svga->crtcreg == 7) && (svga->crtc[0x11] & 0x80))
                val = (svga->crtc[7] & ~0x10) | (val & 0x10);
            old                       = svga->crtc[svga->crtcreg];
            svga->crtc[svga->crtcreg] = val;

            if (old != val) {
                if (svga->crtcreg < 0xe || svga->crtcreg > 0x10) {
                    if ((svga->crtcreg == 0xc) || (svga->crtcreg == 0xd)) {
                        svga->fullchange = 3;
                        svga->memaddr_latch   = ((svga->crtc[0xc] << 8) | svga->crtc[0xd]) + ((svga->crtc[8] & 0x60) >> 5);
                    } else {
                        svga->fullchange = svga->monitor->mon_changeframecount;
                        svga_recalctimings(svga);
                    }
                }
            }
            break;

        default:
            break;
    }
    svga_out(addr, val, svga);
}

uint8_t
atirage_in(uint16_t addr, void *priv)
{
    atirage_t *atirage = priv;
    svga_t   *svga   = &atirage->svga;

    if (((addr & 0xFFF0) == 0x3D0 || (addr & 0xFFF0) == 0x3B0) && !(svga->miscout & 1))
        addr ^= 0x60;

    switch (addr) {
        case 0x1ce:
            return atirage->index;
        case 0x1cf:
            return atirage->regs[atirage->index & 0x3f];
        case 0x3C6 ... 0x3C9:
            return svga_in(addr, svga);
        case 0x3D4:
            return svga->crtcreg;
        case 0x3D5:
            if (svga->crtcreg > 0x20)
                return 0xff;
            return svga->crtc[svga->crtcreg];

        default:
            break;
    }
    return svga_in(addr, svga);
}

void
atirage_recalctimings(svga_t *svga)
{
    const atirage_t *atirage = (atirage_t *) svga->priv;

    if (((atirage->crtc_gen_cntl >> 24) & 3) == 3) {
        svga->char_width = 8;
        svga->vtotal     = (atirage->crtc_v_total_disp & 2047) + 1;
        svga->dispend    = ((atirage->crtc_v_total_disp >> 16) & 2047) + 1;
        svga->htotal     = (atirage->crtc_h_total_disp & 255) + 1;
        svga->hdisp_time = svga->hdisp = ((atirage->crtc_h_total_disp >> 16) & 255) + 1;
        svga->hblankstart              = (atirage->crtc_h_sync_strt_wid & 255) +
                                         ((atirage->crtc_h_sync_strt_wid >> 8) & 7);
        svga->hblank_end_val           = (svga->hblankstart +
                                         ((atirage->crtc_h_sync_strt_wid >> 16) & 31) - 1) & 63;
        svga->vsyncstart               = (atirage->crtc_v_sync_strt_wid & 2047) + 1;
        svga->rowoffset                = (atirage->crtc_off_pitch >> 22);
        svga->clock                    = (cpuclock * (double) (1ULL << 32)) / ics2595_getclock(svga->clock_gen);
        svga->memaddr_latch            = (atirage->crtc_off_pitch & 0x1fffff) * 2;
        svga->linedbl = svga->rowcount = 0;
        svga->split                    = 0xffffff;
        svga->vblankstart              = svga->dispend;
        svga->rowcount                 = atirage->crtc_gen_cntl & 1;
        svga->lut_map                  = 1;
        svga->rowoffset <<= 1;
        svga->attrregs[0x13]          &= ~0x0f;

        svga->packed_4bpp = !!(((atirage->crtc_gen_cntl >> 8) & 7) == BPP_4);

        switch ((atirage->crtc_gen_cntl >> 8) & 7) {
            case BPP_4:
                svga->render = svga_render_4bpp_highres;
                svga->hdisp <<= 3;
                svga->bpp = 4;
                break;
            case BPP_8:
                svga->render = svga_render_8bpp_clone_highres;
                svga->hdisp <<= 3;
                svga->rowoffset >>= 1;
                svga->bpp = 8;
                break;
            case BPP_15:
                svga->render = svga_render_15bpp_highres;
                svga->hdisp <<= 3;
                svga->bpp = 15;
                break;
            case BPP_16:
                svga->render = svga_render_16bpp_highres;
                svga->hdisp <<= 3;
                svga->bpp = 16;
                break;
            case BPP_24:
                svga->render = svga_render_24bpp_highres;
                svga->hdisp <<= 3;
                svga->rowoffset = (svga->rowoffset * 3) >> 1;
                svga->bpp = 24;
                break;
            case BPP_32:
                svga->render = svga_render_32bpp_highres;
                svga->hdisp <<= 3;
                svga->rowoffset <<= 1;
                svga->bpp = 32;
                break;

            default:
                break;
        }

        svga->vram_display_mask = atirage->vram_mask;
    } else {
        svga->vram_display_mask = (atirage->regs[0x36] & 0x01) ? atirage->vram_mask : 0x3ffff;
        svga->lut_map           = 0;
        svga->bpp               = 8;
    }
}

void
atirage_updatemapping(atirage_t *atirage)
{
    svga_t *svga = &atirage->svga;
    xga_t *xga   = (xga_t *) svga->xga;

    if (atirage->pci && !(atirage->pci_regs[PCI_REG_COMMAND] & PCI_COMMAND_MEM)) {
        // pclog( "Update mapping - PCI disabled\n");
        mem_mapping_disable(&svga->mapping);
        mem_mapping_disable(&atirage->linear_mapping);
        mem_mapping_disable(&atirage->linear_mapping_big_endian);
        mem_mapping_disable(&atirage->mmio_mapping);
        mem_mapping_disable(&atirage->mmio_linear_mapping);
        mem_mapping_disable(&atirage->mmio_linear_mapping_2);
        return;
    }

    mem_mapping_disable(&atirage->mmio_mapping);
    switch (svga->gdcreg[6] & 0xc) {
        case 0x0: /*128k at A0000*/
            mem_mapping_set_handler(&svga->mapping, atirage_read, atirage_readw, atirage_readl, atirage_write, atirage_writew, atirage_writel);
            mem_mapping_set_p(&svga->mapping, atirage);
            mem_mapping_set_addr(&svga->mapping, 0xa0000, 0x20000);
            mem_mapping_enable(&atirage->mmio_mapping);
            svga->banked_mask = 0xffff;
            break;
        case 0x4: /*64k at A0000*/
            mem_mapping_set_handler(&svga->mapping, atirage_read, atirage_readw, atirage_readl, atirage_write, atirage_writew, atirage_writel);
            mem_mapping_set_p(&svga->mapping, atirage);
            mem_mapping_set_addr(&svga->mapping, 0xa0000, 0x10000);
            svga->banked_mask = 0xffff;
            if (xga_active && (svga->xga != NULL))
                xga->on = 0;
            break;
        case 0x8: /*32k at B0000*/
            mem_mapping_set_handler(&svga->mapping, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel);
            mem_mapping_set_p(&svga->mapping, svga);
            mem_mapping_set_addr(&svga->mapping, 0xb0000, 0x08000);
            svga->banked_mask = 0x7fff;
            break;
        case 0xC: /*32k at B8000*/
            mem_mapping_set_handler(&svga->mapping, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel);
            mem_mapping_set_p(&svga->mapping, svga);
            mem_mapping_set_addr(&svga->mapping, 0xb8000, 0x08000);
            svga->banked_mask = 0x7fff;
            break;

        default:
            break;
    }


    /*
       The small aperture is only available in accelerated modes and only if
       bit 2 of the Config_Cntl (6AECh) register is set.
     */
    if (atirage->config_cntl & 4) {
        mem_mapping_set_handler(&svga->mapping, atirage_read, atirage_readw, atirage_readl, atirage_write, atirage_writew, atirage_writel);
        mem_mapping_set_p(&svga->mapping, atirage);
        mem_mapping_enable(&atirage->mmio_mapping);
    } else {
        mem_mapping_set_handler(&svga->mapping, svga_read, svga_readw, svga_readl, svga_write, svga_writew, svga_writel);
        mem_mapping_set_p(&svga->mapping, svga);
        mem_mapping_disable(&atirage->mmio_mapping);
    }


    if (atirage->linear_base) {
        /*2*8 MB aperture*/
        mem_mapping_set_addr(&atirage->linear_mapping, atirage->linear_base, (8 << 20) - 4096);
        mem_mapping_set_addr(&atirage->mmio_linear_mapping, atirage->linear_base + ((8 << 20) - 4096), 4096);
        mem_mapping_set_addr(&atirage->linear_mapping_big_endian, atirage->linear_base + (8 << 20), (8 << 20) - 0x1000);
        mem_mapping_set_addr(&atirage->mmio_linear_mapping_2, atirage->linear_base + ((16 << 20) - 0x1000), 0x1000);
    } else {
        mem_mapping_disable(&atirage->linear_mapping);
        mem_mapping_disable(&atirage->mmio_linear_mapping);
        mem_mapping_disable(&atirage->mmio_linear_mapping_2);
        mem_mapping_disable(&atirage->linear_mapping_big_endian);
    }
}

static void
atirage_update_irqs(atirage_t *atirage)
{
    if (!atirage->pci)
        return;

    if ((atirage->crtc_int_cntl & 0xaa0024) & ((atirage->crtc_int_cntl << 1) & 0xaa0024))
        pci_set_irq(atirage->pci_slot, PCI_INTA, &atirage->irq_state);
    else
        pci_clear_irq(atirage->pci_slot, PCI_INTA, &atirage->irq_state);
}


#define PLL_REF_DIV   0x2
#define VCLK_POST_DIV 0x6
#define VCLK0_FB_DIV  0x7

static void
pll_write(atirage_t *atirage, uint32_t addr, uint8_t val)
{
    switch (addr & 3) {
        case 0: /*Clock sel*/
            break;
        case 1: /*Addr*/
            atirage->pll_addr = (val >> 2) & 0xf;
            break;
        case 2: /*Data*/
            atirage->pll_regs[atirage->pll_addr] = val;
            // pclog( "pll_write %02x,%02x\n", atirage->pll_addr, val);

            for (uint8_t c = 0; c < 4; c++) {
                double m = (double) atirage->pll_regs[PLL_REF_DIV];
                double n = (double) atirage->pll_regs[VCLK0_FB_DIV + c];
                double r = 14318184.0;
                double p = (double) (1 << ((atirage->pll_regs[VCLK_POST_DIV] >> (c * 2)) & 3));

                // pclog( "PLLfreq %i = %g  %g m=%02x n=%02x p=%02x\n", c, (2.0 * r * n) / (m * p), p, atirage->pll_regs[PLL_REF_DIV], atirage->pll_regs[VCLK0_FB_DIV + c], atirage->pll_regs[VCLK_POST_DIV]);
                atirage->pll_freq[c] = (2.0 * r * n) / (m * p);
                // pclog( " %g\n", atirage->pll_freq[c]);
            }
            break;

        default:
            break;
    }
}



#define OVERLAY_EN (1 << 30)
static void
atirage_vblank_start(svga_t *svga)
{
    atirage_t *atirage          = (atirage_t *) svga->priv;
    int       overlay_cmp_mix = (atirage->overlay_key_cntl >> 8) & 0xf;

    atirage->crtc_int_cntl |= 4;
    atirage_update_irqs(atirage);

    svga->overlay.x = (atirage->overlay_y_x_start >> 16) & 0x7ff;
    svga->overlay.y = atirage->overlay_y_x_start & 0x7ff;

    svga->overlay.cur_xsize = ((atirage->overlay_y_x_end >> 16) & 0x7ff) - svga->overlay.x;
    svga->overlay.cur_ysize = (atirage->overlay_y_x_end & 0x7ff) - svga->overlay.y;

    if (atirage->type >= MACH64_GTB) {
        svga->overlay.addr  = atirage->scaler_buf_offset[0] & 0x3fffff;
        svga->overlay.pitch = atirage->scaler_buf_pitch & 0xfff;
    } else {
        svga->overlay.addr  = atirage->buf_offset[0] & 0x3ffff8;
        svga->overlay.pitch = atirage->buf_pitch[0] & 0xfff;
    }

    svga->overlay.ena = (atirage->overlay_scale_cntl & OVERLAY_EN) && (overlay_cmp_mix != 1);

    atirage->overlay_v_acc   = 0;
    atirage->scaler_update   = 1;
    atirage->overlay_uv_addr = svga->overlay.addr;
    atirage->overlay_cur_y   = 0;
    atirage->overlay_base    = svga->overlay.addr;
}

uint8_t
atirage_ext_readb(uint32_t addr, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;
    svga_t   *svga   = &atirage->svga;

    uint8_t ret = 0xff;

    if ((addr >= 0x000a0000) && (addr < 0x000bf800))
        return svga->mapping.read_b(addr, svga->mapping.priv);
    if (!((addr < 0x000a0000) || ((addr >= 0x000bf800) && (addr <= 0x000bffff)) || (addr >= 0x00100000)))
        return ret;

    if (!(addr & 0x400)) {
        // pclog( "atirage_ext_readb: addr=%04x\n", addr);
        switch (addr & 0x3ff) {
            case 0x00 ... 0x03:
                READ8(addr, atirage->overlay_y_x_start);
                break;
            case 0x04 ... 0x07:
                READ8(addr, atirage->overlay_y_x_end);
                break;
            case 0x08 ... 0x0b:
                READ8(addr, atirage->overlay_video_key_clr);
                break;
            case 0x0c ... 0x0f:
                READ8(addr, atirage->overlay_video_key_msk);
                break;
            case 0x10 ... 0x13:
                READ8(addr, atirage->overlay_graphics_key_clr);
                break;
            case 0x14 ... 0x17: 
                READ8(addr, atirage->overlay_graphics_key_msk);
                break;
            case 0x18 ... 0x1b:
                READ8(addr, atirage->overlay_key_cntl);
                break;
            case 0x20 ... 0x23:
                READ8(addr, atirage->overlay_scale_inc);
                break;
            case 0x24 ... 0x27:
                READ8(addr, atirage->overlay_scale_cntl);
                break;
            case 0x28 ... 0x2b:
                READ8(addr, atirage->scaler_height_width);
                break;
            #if 0
            case 0x2c ... 0x2f:
                warning("Attempt to read from OVERLAY_TEST addr=%08X",addr);
                break;
            case 0x30 ... 0x33:
                warning("Attempt to read from SCALER_THRESHOLD addr=%08X",addr);
                break;
            #endif
            case 0x34 ... 0x37:
                READ8(addr, atirage->scaler_buf_offset[0]);
                break;
            case 0x38 ... 0x3b: // optimise
                READ8(addr, atirage->scaler_buf_offset[1]);
                break;
            case 0x3c ... 0x3f:
                READ8(addr, atirage->scaler_buf_pitch);
                break;
            #if 0
            case 0x40 ... 0x43:
                warning("Attempt to read from CAPTURE_Y_X addr=%08X",addr);
                break;
            case 0x44 ... 0x47:
                warning("Attempt to read from CAPTURE_HEIGHT_WIDTH addr=%08X",addr);
                break;
            case 0x4c ... 0x4f:
                warning("Attempt to read from VIDEO_CONFIG addr=%08X",addr);
                break;
            case 0x50 ... 0x53:
                warning("Attempt to read from CAPTURE_CONFIG addr=%08X",addr);
                break;
            case 0x54 ... 0x57:
                warning("Attempt to read from TRIG_CNTL addr=%08X",addr);
                break;
            #endif
            case 0x58 ... 0x5b:
                READ8(addr, atirage->overlay_exclusive_horz);
                break;
            case 0x5c ... 0x5f:
                READ8(addr, atirage->overlay_exclusive_vert);
                break;
            #if 0
            case 0x60 ... 0x63:
                warning("Attempt to read from VMC_CONFIG addr=%08X",addr);
                break;
            case 0x64 ... 0x67:
                warning("Attempt to read from VMC_STATUS addr=%08X",addr);
                break;
            case 0x68 ... 0x6b:
                warning("Attempt to read from VMC_CMD addr=%08X",addr);
                break;
            case 0x6c ... 0x6f:
                warning("Attempt to read from VMC_ARG0 addr=%08X",addr);
                break;
            case 0x70 ... 0x73:
                warning("Attempt to read from VMC_ARG1 addr=%08X",addr);
                break;
            case 0x74 ... 0x77:
                warning("Attempt to read from VMC_SNOOP_ARG0 addr=%08X",addr);
                break;
            case 0x78 ... 0x7b:
                warning("Attempt to read from VMC_SNOOP_ARG1 addr=%08X",addr);
                break;
            case 0x80 ... 0x83:
                warning("Attempt to read from BUF0_OFFSET addr=%08X",addr);
                break;
            case 0x8c ... 0x8f:
                warning("Attempt to read from BUF0_PITCH addr=%08X",addr);
                break;
            case 0x98 ... 0x9b:
                warning("Attempt to read from BUF1_OFFSET addr=%08X",addr);
                break;
            case 0xa4 ... 0xa7:
                warning("Attempt to read from BUF1_PITCH addr=%08X",addr);
                break;
            case 0xac ... 0xaf:
                warning("Attempt to read from BUF0_CAP_ODD_OFFSET addr=%08X",addr);
                break;
            case 0xb0 ... 0xb3:
                warning("Attempt to read from BUF1_CAP_ODD_OFFSET addr=%08X",addr);
                break;
            case 0xc0 ... 0xff:
                warning("Attempt to read from VMC_STRM_DATA addr=%08X",addr);
                break;
            case 0x140 ... 0x143:
                warning("Attempt to read from HW_DEBUG addr=%08X",addr);
                break;
            #endif
            case 0x4a:
                ret = atirage->scaler_format;
                break;
            case 0x4b:
                ret = atirage->scaler_yuv_aper;
                break;
            #if 0
            case 0x48 ... 0x49:
                warning("Attempt to read from VIDEO_FORMAT addr=%08X",addr);
                break;
            #endif
            default:
                ret = 0xff;
                break;
        }
    } else {
        switch (addr & 0x3ff) {
            case 0x00 ... 0x03:
                READ8(addr, atirage->crtc_h_total_disp);
                break;
            case 0x04 ... 0x07:
                READ8(addr, atirage->crtc_h_sync_strt_wid);
                break;
            case 0x08 ... 0x0b:
                READ8(addr, atirage->crtc_v_total_disp);
                break;
            case 0x0c ... 0x0f:
                READ8(addr, atirage->crtc_v_sync_strt_wid);
                break;
            case 0x12 ... 0x13:
                READ8(addr - 2, atirage->svga.vc);
                break;
            case 0x14 ... 0x17:
                READ8(addr, atirage->crtc_off_pitch);
                break;
            case 0x18:
                ret = atirage->crtc_int_cntl & ~1;
                if (atirage->svga.cgastat & 8)
                    ret |= 1;
                break;
            case 0x1c ... 0x1f:
                READ8(addr, atirage->crtc_gen_cntl);
                break;
            case 0x20 ... 0x23:
                READ8(addr, atirage->dsp_config);
                break;
            case 0x24 ... 0x27:
                READ8(addr, atirage->dsp_on_off);
                break;
            case 0x40 ... 0x43: 
                READ8(addr, atirage->ovr_clr);
                break;
            case 0x44 ... 0x47:
                READ8(addr, atirage->ovr_wid_left_right);
                break;
            case 0x48 ... 0x4b:
                READ8(addr, atirage->ovr_wid_top_bottom);
                break;
            case 0x4c ... 0x4f:
                READ8(addr, atirage->vga_dsp_config);
                break;
            case 0x50 ... 0x53:
                READ8(addr, atirage->vga_dsp_on_off);
                break;
            case 0x60 ... 0x63:
                READ8(addr, atirage->cur_clr0);
                break;
            case 0x64 ... 0x67:
                READ8(addr, atirage->cur_clr1);
                break;
            case 0x68 ... 0x6b:
                READ8(addr, atirage->cur_offset);
                break;
            case 0x6c ... 0x6f:
                READ8(addr, atirage->cur_horz_vert_posn);
                break;
            case 0x70 ... 0x73:
                READ8(addr, atirage->cur_horz_vert_off);
                break;
            case 0x79:
                ret = 0x30;
                if (atirage->type == MACH64_GTB)
                {
                    ret = (((i2c_gpio_get_scl(atirage->i2c_tv) << 3)) & (~(atirage->gp_io >> 24) & 0xFF)) | ((atirage->gp_io >> 8) & (atirage->gp_io >> 24) & 0xFF);
                    ret &= ~((1 << 4) | (1 << 5));
                    ret |= (i2c_gpio_get_scl(atirage->i2c) << 5) | (i2c_gpio_get_sda(atirage->i2c) << 4);
                    break;
                }
                break;
            case 0x78:
                if (atirage->type == MACH64_GTB)
                {
                    ret = ((i2c_gpio_get_sda(atirage->i2c_tv) << 4) & (~(atirage->gp_io >> 16) & 0xFF)) | ((atirage->gp_io & 0xFF) & ((atirage->gp_io >> 16) & 0xFF));
                    break;
                }
            case 0x7A ... 0x7B:
                if (atirage->type == MACH64_GTB)
                    READ8(addr, atirage->gp_io);
//                // pclog( "GPIO READ 0x%X, 0x00\n", addr & 0x3ff);
                break;
            case 0x80 ... 0x83:
                READ8(addr, atirage->scratch_reg0);
                break;
            case 0x84 ... 0x87:
                READ8(addr, atirage->scratch_reg1);
                break;
            case 0x90 ... 0x93:
                READ8(addr, atirage->clock_cntl);
                break;
            case 0xa0 ... 0xa3:
                #ifdef USE_RAGE_BUS_CNTL
                READ8(addr, atirage->bus_cntl);
                #endif
                break;
            case 0xb0 ... 0xb3:
                READ8(addr, atirage->mem_cntl);
                break;
            case 0xc0 ... 0xc3:
                uint16_t port_list[4] = { 0x3c8, 0x3c9, 0x3c6, 0x3c7 }; 
                ret = svga_in(port_list[addr & 3], svga);
                break;
            case 0xc4 ... 0xc6: // optimise
                READ8(addr, atirage->dac_cntl);
                break;
            case 0xc7:
                READ8(addr, atirage->dac_cntl);
                if (atirage->type >= MACH64_GT && atirage->type != MACH64_GTB) {
                    ret &= 0xf9;
                    if (i2c_gpio_get_scl(atirage->i2c))
                        ret |= 0x04;
                    if (i2c_gpio_get_sda(atirage->i2c))
                        ret |= 0x02;
                }
                break;
            case 0xd0 ... 0xd3:
                READ8(addr, atirage->gen_test_cntl);
                break;
            case 0xdc ... 0xdf:
                atirage->config_cntl = (atirage->config_cntl & ~0x3fc0) | ((atirage->linear_base >> 24) << 6);
                READ8(addr, atirage->config_cntl);
                break;
            case 0xe0 ... 0xe3:
                READ8(addr, atirage->config_chip_id);
                break;
            case 0xe4 ... 0xe7:
                READ8(addr, atirage->config_stat0);
                break;
            case 0x100 ... 0x103:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->dst_off_pitch);
                break;
            case 0x104 ... 0x105:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->dst_y_x);
                break;
            case 0x108 ... 0x109:
            case 0x11c ... 0x11d:
                atirage_wait_fifo_idle(atirage);
                READ8(addr + 2, atirage->dst_y_x); // optimise
                break;
            case 0x10c ... 0x10f: /* 0_43h DST_Y_X */
            case 0x134 ... 0x137: /* 0_4Dh DST_Y_X Rage alias */
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->dst_y_x);
                break;
            case 0x2e8 ... 0x2eb:
                atirage_wait_fifo_idle(atirage);
                READ8(addr ^ 2, atirage->dst_y_x);
                break;
            case 0x2ec ... 0x2ef:
                atirage_wait_fifo_idle(atirage);
                READ8(addr ^ 2, atirage->dst_height_width);
                break;
            case 0x110 ... 0x111: // optimise
                addr += 2;
                fallthrough;
                // probably can be optimised but might be a little slower?
            case 0x114 ... 0x115:
            case 0x118 ... 0x119:
            case 0x11a ... 0x11b:
            case 0x11e ... 0x11f:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->dst_height_width);
                break;
            case 0x120 ... 0x123: /* 0_48h DST_BRES_LNTH */
            case 0x144 ... 0x147: /* 0_51h DST_BRES_LNTH Rage alias, also known as LEAD_BRES_LNTH */
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->dst_bres_lnth);
                break;
            case 0x124 ... 0x127:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->dst_bres_err);
                break;
            case 0x128 ... 0x12b:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->dst_bres_inc);
                break;
            case 0x12c ... 0x12f:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->dst_bres_dec);
                break;
            case 0x130 ... 0x133:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->dst_cntl);
                break;
            case 0x138 ... 0x13b: /* 0_4Eh TRAIL_BRES_ERR, Rage only*/
                READ8(addr, atirage->trail_bres_err);
                break;
            case 0x13c ... 0x13f: /* 0_4Fh TRAIL_BRES_INC, Rage only*/
                READ8(addr, atirage->trail_bres_inc);
                break;
            case 0x140 ... 0x143: /* 0_50h TRAIL_BRES_DEC, Rage only*/
                READ8(addr, atirage->trail_bres_dec);
                break;
            case 0x148 ... 0x14b:
                warning("Attempt to read from Z_OFF_PITCH addr=%03X",addr);
                READ8(addr, atirage->z_off_pitch);
                break;
            case 0x14c ... 0x14f:
                warning("Attempt to read from Z_CNTL addr=%03X",addr);
                READ8(addr, atirage->z_cntl);
                break;
            case 0x180 ... 0x183:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->src_off_pitch);
                break;
            case 0x184 ... 0x185:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->src_y_x);
                break;
            case 0x188 ... 0x189:
                atirage_wait_fifo_idle(atirage);
                READ8(addr + 2, atirage->src_y_x);
                break;
            case 0x18c ... 0x18f:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->src_y_x);
                break;
            case 0x190 ... 0x191:
                atirage_wait_fifo_idle(atirage);
                READ8(addr + 2, atirage->src_height1_width1);
                break;
            case 0x194 ... 0x195:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->src_height1_width1);
                break;
            case 0x198 ... 0x19b:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->src_height1_width1);
                break;
            case 0x19c ... 0x19d:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->src_y_x_start);
                break;
            case 0x1a0 ... 0x1a1:
                atirage_wait_fifo_idle(atirage);
                READ8(addr + 2, atirage->src_y_x_start);
                break;
            case 0x1a4 ... 0x1a7:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->src_y_x_start);
                break;
            case 0x1a8 ... 0x1a9:
                atirage_wait_fifo_idle(atirage);
                READ8(addr + 2, atirage->src_height2_width2);
                break;
            case 0x1ac ... 0x1ad:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->src_height2_width2);
                break;
            case 0x1b0 ... 0x1b3:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->src_height2_width2);
                break;
            case 0x1b4 ... 0x1b7:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->src_cntl);
                break;
            case 0x1c0 ... 0x1c3:
                warning("Attempt to read from SCALE_Y_OFF or TEX_0_OFF addr=%08X",addr);
                READ8(addr, atirage->tex_0_off);
                break;
            case 0x1c4 ... 0x1c7:
                warning("Attempt to read from TEX_1_OFF addr=%08X",addr);
                READ8(addr, atirage->tex_1_off);
                break;
            case 0x1c8 ... 0x1cb:
                warning("Attempt to read from TEX_2_OFF addr=%08X",addr);
                READ8(addr, atirage->tex_2_off);
                break;
            case 0x1cc ... 0x1cf:
                warning("Attempt to read from TEX_3_OFF addr=%08X",addr);
                READ8(addr, atirage->tex_3_off);
                break;
            case 0x1d0 ... 0x1d3:
                warning("Attempt to read from TEX_4_OFF addr=%08X",addr);
                READ8(addr, atirage->tex_4_off);
                break;
            case 0x1d4 ... 0x1d7:
                warning("Attempt to read from TEX_5_OFF addr=%08X",addr);
                READ8(addr, atirage->tex_5_off);
                break;
            case 0x1d8 ... 0x1db:
                warning("Attempt to read from TEX_6_OFF addr=%08X",addr);
                READ8(addr, atirage->tex_6_off);
                break;
            case 0x1dc ... 0x1df:
                warning("Attempt to read from SCALE_WIDTH or TEX_7_OFF addr=%08X",addr);
                READ8(addr, atirage->tex_7_off);
                break;
            case 0x1e0 ... 0x1e3:
                warning("Attempt to read from SCALE_HEIGHT or TEX_8_OFF addr=%08X",addr);
                READ8(addr, atirage->tex_8_off);
                break;
            case 0x1e4 ... 0x1e7:
                warning("Attempt to read from TEX_9_OFF addr=%08X",addr);
                READ8(addr, atirage->tex_9_off);
                break;
            case 0x1e8 ... 0x1eb:
                warning("Attempt to read from TEX_10_OFF addr=%08X",addr);
                READ8(addr, atirage->tex_10_off);
                break;
            case 0x1ec ... 0x1ef:
            case 0x350 ... 0x353: /* 0_7Bh is aliased to 0_D4h */
                warning("Attempt to read from S_Y_INC or SCALE_Y_PITCH addr=%08X",addr);
                READ8(addr, atirage->s_y_inc);
                break;
            case 0x1f0 ... 0x1f3:
            case 0x3c0 ... 0x3c3: /* 0_7Ch is aliased to 0_F0h */
                warning("Attempt to read from SCALE_X_INC or RED_X_INC addr=%08X",addr);
                READ8(addr, atirage->red_x_inc);
                break;
            case 0x1f4 ... 0x1f7:
            case 0x3cc ... 0x3cf: /* 0_7Dh is aliased to 0_F3h */
                warning("Attempt to read from SCALE_Y_INC or GREEN_X_INC addr=%08X",addr);
                READ8(addr, atirage->green_x_inc);
                break;
            case 0x1f8 ... 0x1fb:
                warning("Attempt to read from SCALE_VACC addr=%08X",addr);
                READ8(addr, atirage->scale_vacc);
                break;
            case 0x1fc ... 0x1ff:
                warning("Attempt to read from SCALE_3D_CNTL addr=%08X",addr);
                READ8(addr, atirage->scale_3d_cntl);
                break;
            case 0x240 ... 0x243:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->host_cntl);
                break;
            case 0x280 ... 0x283:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->pat_reg0);
                break;
            case 0x284 ... 0x287:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->pat_reg1);
                break;
            case 0x288 ... 0x28b:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->pat_cntl);
                break;
            case 0x2a0 ... 0x2a1:
            case 0x2a8 ... 0x2a9: // optimise
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->sc_left_right);
                break;
            case 0x2a4 ... 0x2a5:
                addr += 2;
                fallthrough;
            case 0x2aa ... 0x2ab:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->sc_left_right);
                break;
            case 0x2ac ... 0x2ad:
            case 0x2b4 ... 0x2b5:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->sc_top_bottom);
                break;
            case 0x2b0 ... 0x2b1:
                addr += 2;
                fallthrough;
            case 0x2b6 ... 0x2b7:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->sc_top_bottom);
                break;
            case 0x2c0 ... 0x2c3:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->dp_bkgd_clr);
                break;
            case 0x2c4 ... 0x2c7:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->dp_frgd_clr);
                break;
            case 0x2c8 ... 0x2cb:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->write_mask);
                break;
            case 0x2cc ... 0x2cf:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->chain_mask);
                break;
            case 0x2d0 ... 0x2d3:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->dp_pix_width);
                break;
            case 0x2d4 ... 0x2d7:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->dp_mix);
                break;
            case 0x2d8 ... 0x2db:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->dp_src);
                break;
            case 0x300 ... 0x303:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->clr_cmp_clr);
                break;
            case 0x304 ... 0x307:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->clr_cmp_mask);
                break;
            case 0x308 ... 0x30b:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->clr_cmp_cntl);
                break;
            case 0x310 ... 0x311:
                if (!atirage->blitter_busy)
                    atirage_wake_fifo_thread(atirage);

                ret = 0;
                if (FIFO_FULL)
                    ret = 0xff;
                break;
            case 0x320 ... 0x323:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->context_mask);
                break;
            case 0x330 ... 0x331:
                atirage_wait_fifo_idle(atirage);
                READ8(addr, atirage->dst_cntl);
                break;
            case 0x332:
                atirage_wait_fifo_idle(atirage);
                READ8(addr - 2, atirage->src_cntl);
                break;
            case 0x333:
                atirage_wait_fifo_idle(atirage);
                READ8(addr - 3, atirage->pat_cntl);
                break;
            case 0x338:
                if (!atirage->blitter_busy)
                    atirage_wake_fifo_thread(atirage);
                ret = FIFO_EMPTY ? 0 : 1;
                break;
            case 0x33a:
                ret = FIFO_EMPTY ? 32 : 31;
                break;

            case 0x340 ... 0x343:
                warning("Attempt to read from S_X_INC2 addr=%08X",addr);
                READ8(addr, atirage->s_x_inc2);
                break;
            case 0x344 ... 0x347:
                warning("Attempt to read from S_Y_INC2 addr=%08X",addr);
                READ8(addr, atirage->s_y_inc2);
                break;
            case 0x348 ... 0x34b:
                warning("Attempt to read from S_XY_INC2 addr=%08X",addr);
                READ8(addr, atirage->s_xy_inc2);
                break;
            case 0x34c ... 0x34f:
                warning("Attempt to read from S_XINC_START addr=%08X",addr);
                READ8(addr, atirage->s_xinc_start);
                break;
            case 0x354 ... 0x357:
                warning("Attempt to read from S_START addr=%08X",addr);
                READ8(addr, atirage->s_start);
                break;
            case 0x358 ... 0x35b:
                warning("Attempt to read from T_X_INC2 addr=%08X",addr);
                READ8(addr, atirage->t_x_inc2);
                break;
            case 0x35c ... 0x35f:
                warning("Attempt to read from T_Y_INC2 addr=%08X",addr);
                READ8(addr, atirage->t_y_inc2);
                break;
            case 0x360 ... 0x363:
                warning("Attempt to read from T_XY_INC2 addr=%08X",addr);
                READ8(addr, atirage->t_xy_inc2);
                break;
            case 0x364 ... 0x367:
                warning("Attempt to read from T_XINC_START addr=%08X",addr);
                READ8(addr, atirage->t_xinc_start);
                break;
            case 0x368 ... 0x36b:
                warning("Attempt to read from T_Y_INC addr=%08X",addr);
                READ8(addr, atirage->t_y_inc);
                break;
            case 0x36c ... 0x36f:
                warning("Attempt to read from T_START addr=%08X",addr);
                READ8(addr, atirage->t_start);
                break;
            case 0x370 ... 0x373:
                warning("Attempt to read from TEX_SIZE_PITCH addr=%08X",addr);
                READ8(addr, atirage->tex_size_pitch);
                break;
            case 0x3c4 ... 0x3c7:
                warning("Attempt to read from RED_Y_INC addr=%08X",addr);
                READ8(addr, atirage->red_y_inc);
                break;
            case 0x3c8 ... 0x3cb:
                warning("Attempt to read from SCALE_HACC or RED_START addr=%08X",addr);
                READ8(addr, atirage->red_start);
                break;
            case 0x3d0 ... 0x3d3:
                warning("Attempt to read from GREEN_Y_INC addr=%08X",addr);
                READ8(addr, atirage->green_y_inc);
                break;
            case 0x3d4 ... 0x3d7:
                warning("Attempt to read from GREEN_START addr=%08X",addr);
                READ8(addr, atirage->green_start);
                break;
            case 0x3d8 ... 0x3db:
                warning("Attempt to read from SCALE_XUV_INC or BLUE_X_INC addr=%08X",addr);
                READ8(addr, atirage->blue_x_inc);
                break;
            case 0x3dc ... 0x3df:
                warning("Attempt to read from BLUE_Y_INC addr=%08X",addr);
                READ8(addr, atirage->blue_y_inc);
                break;
            case 0x3e0 ... 0x3e3:
                warning("Attempt to read from SCALE_UV_HACC or BLUE_START addr=%08X",addr);
                READ8(addr, atirage->blue_start);
                break;
            case 0x3e4 ... 0x3e7:
                warning("Attempt to read from Z_X_INC addr=%08X",addr);
                READ8(addr, atirage->z_x_inc);
                break;
            case 0x3e8 ... 0x3eb:
                warning("Attempt to read from Z_Y_INC addr=%08X",addr);
                READ8(addr, atirage->z_y_inc);
                break;
            case 0x3ec ... 0x3ef:
                warning("Attempt to read from Z_START addr=%08X",addr);
                READ8(addr, atirage->z_start);
                break;
            case 0x3f0 ... 0x3f3:
                warning("Attempt to read from ALPHA_X_INC or FOG_X_INC addr=%08X",addr);
                READ8(addr, atirage->alpha_x_inc);
                break;
            case 0x3f4 ... 0x3f7:
                warning("Attempt to read from ALPHA_Y_INC or FOG_Y_INC addr=%08X",addr);
                READ8(addr, atirage->alpha_y_inc);
                break;
            case 0x3f8 ... 0x3fb:
                warning("Attempt to read from ALPHA_START or FOG_START addr=%08X",addr);
                READ8(addr, atirage->alpha_start);
                break;
            default:
                ret = 0;
                break;
        }
    }
    if ((addr & 0x3fc) != 0x018)
        ; // pclog( "atirage_ext_readb : addr %08X ret %02X\n", addr, ret);
    return ret;
}
uint16_t
atirage_ext_readw(uint32_t addr, void *priv)
{
    const atirage_t *atirage = (atirage_t *) priv;
    const svga_t    *svga    = &atirage->svga;
    uint16_t         ret     = 0xffff;

    if ((addr >= 0x000a0000) && (addr < 0x000bf800))
        return svga->mapping.read_w(addr, svga->mapping.priv);
    if (!((addr < 0x000a0000) || ((addr >= 0x000bf800) && (addr <= 0x000bffff)) || (addr >= 0x00100000)))
        return ret;

    if (!(addr & 0x400)) {
        // pclog( "atirage_ext_readw: addr=%04x\n", addr);
        ret = atirage_ext_readb(addr, priv);
        ret |= atirage_ext_readb(addr + 1, priv) << 8;
    } else // optimise
        switch (addr & 0x3ff) {
            case 0xb4: case 0xb6:
                ret = (atirage->bank_w[(addr & 2) >> 1] >> 15);
                break;
            case 0xb8: case 0xba:
                ret = (atirage->bank_r[(addr & 2) >> 1] >> 15);
                break;
            default:
                ret = atirage_ext_readb(addr, priv);
                ret |= atirage_ext_readb(addr + 1, priv) << 8;
                break;
        }
    if ((addr & 0x3fc) != 0x018)
        ; // pclog( "atirage_ext_readw : addr %08X ret %04X\n", addr, ret);
    return ret;
}
uint32_t
atirage_ext_readl(uint32_t addr, void *priv)
{
    const atirage_t *atirage = (atirage_t *) priv;
    const svga_t    *svga    = &atirage->svga;
    uint32_t         ret     = 0xffffffff;

    if ((addr >= 0x000a0000) && (addr < 0x000bf800))
        return svga->mapping.read_l(addr, svga->mapping.priv);
    if (!((addr < 0x000a0000) || ((addr >= 0x000bf800) && (addr <= 0x000bffff)) || (addr >= 0x00100000)))
        return ret;

    if (!(addr & 0x400)) {
        // pclog( "atirage_ext_readl: addr=%04x\n", addr);
        ret = atirage_ext_readw(addr, priv);
        ret |= atirage_ext_readw(addr + 2, priv) << 16;
    } else
        switch (addr & 0x3ff) {
            case 0x18:
                ret = atirage->crtc_int_cntl & ~1;
                if (atirage->svga.cgastat & 8)
                    ret |= 1;
                break;
            case 0xb4:
                ret = (atirage->bank_w[0] >> 15) | ((atirage->bank_w[1] >> 15) << 16);
                break;
            case 0xb8:
                ret = (atirage->bank_r[0] >> 15) | ((atirage->bank_r[1] >> 15) << 16);
                break;
            default:
                ret = atirage_ext_readw(addr, priv);
                ret |= atirage_ext_readw(addr + 2, priv) << 16;
                break;
        }
    if ((addr & 0x3fc) != 0x018)
        ; // pclog( "atirage_ext_readl : addr %08X ret %08X\n", addr, ret);
    return ret;
}

void
atirage_ext_writeb(uint32_t addr, uint8_t val, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;
    svga_t   *svga   = &atirage->svga;

    if ((addr >= 0x000a0000) && (addr < 0x000bf800)) {
        svga->mapping.write_b(addr, val, svga->mapping.priv);
        return;
    }
    if (!((addr < 0x000a0000) || ((addr >= 0x000bf800) && (addr <= 0x000bffff)) || (addr >= 0x00100000)))
        return;

    // pclog( "atirage_ext_writeb : addr %08X val %02X\n", addr, val);

    if (!(addr & 0x400)) {
        switch (addr & 0x3ff) {
            case 0x00 ... 0x03:
                WRITE8(addr, atirage->overlay_y_x_start, val);
                break;
            case 0x04 ... 0x07:
                WRITE8(addr, atirage->overlay_y_x_end, val);
                break;
            case 0x08 ... 0x0b:
                WRITE8(addr, atirage->overlay_video_key_clr, val);
                break;
            case 0x0c ... 0x0f:
                WRITE8(addr, atirage->overlay_video_key_msk, val);
                break;
            case 0x10 ... 0x13:
                WRITE8(addr, atirage->overlay_graphics_key_clr, val);
                break;
            case 0x14 ... 0x17:
                WRITE8(addr, atirage->overlay_graphics_key_msk, val);
                break;
            case 0x18 ... 0x1b:
                WRITE8(addr, atirage->overlay_key_cntl, val);
                break;
            case 0x20 ... 0x23:
                WRITE8(addr, atirage->overlay_scale_inc, val);
                break;
            case 0x24 ... 0x27:
                WRITE8(addr, atirage->overlay_scale_cntl, val);
                break;
            case 0x28 ... 0x2b:
                WRITE8(addr, atirage->scaler_height_width, val);
                break;
            #if 0
            case 0x2c ... 0x2f:
                warning("Attempt to write to OVERLAY_TEST addr=%08X, val=%02X",addr, val);
                break;
            case 0x30 ... 0x33:
                warning("Attempt to write to SCALER_THRESHOLD addr=%08X, val=%02X",addr, val);
                break;
            #endif
            case 0x34 ... 0x37:
                WRITE8(addr, atirage->scaler_buf_offset[0], val);
                break;
            case 0x38 ... 0x3b: // optimise
                WRITE8(addr, atirage->scaler_buf_offset[1], val);
                break;
            case 0x3c ... 0x3f:
                WRITE8(addr, atirage->scaler_buf_pitch, val);
                break;
            #if 0
            case 0x40 ... 0x43:
                warning("Attempt to write to CAPTURE_Y_X addr=%08X, val=%02X",addr, val);
                break;
            case 0x44 ... 0x47:
                warning("Attempt to write to CAPTURE_HEIGHT_WIDTH addr=%08X, val=%02X",addr, val);
                break;
            case 0x48 ... 0x49:
                warning("Attempt to write to VIDEO_FORMAT addr=%08X, val=%02X",addr, val);
                break;
            #endif
            case 0x4a:
                atirage->scaler_format = val & 0xf;
                break;
            case 0x4b:
                atirage->scaler_yuv_aper = val;
                break;
            #if 0
            case 0x4c ... 0x4f:
                warning("Attempt to write to VIDEO_CONFIG addr=%08X, val=%02X",addr, val);
                break;
            case 0x50 ... 0x53:
                warning("Attempt to write to CAPTURE_CONFIG addr=%08X, val=%02X",addr, val);
                break;
            case 0x54 ... 0x57:
                warning("Attempt to write to TRIG_CNTL addr=%08X, val=%02X",addr, val);
                break;
            #endif
            case 0x58 ... 0x5b:
                WRITE8(addr, atirage->overlay_exclusive_horz, val);
                break;
            case 0x5c ... 0x5f:
                WRITE8(addr, atirage->overlay_exclusive_vert, val);
                break;
            #if 0
            case 0x60 ... 0x63:
                warning("Attempt to write to VMC_CONFIG addr=%08X, val=%02X",addr, val);
                break;
            case 0x64 ... 0x67:
                warning("Attempt to write to VMC_STATUS addr=%08X, val=%02X",addr, val);
                break;
            case 0x68 ... 0x6b:
                warning("Attempt to write to VMC_CMD addr=%08X, val=%02X",addr, val);
                break;
            case 0x6c ... 0x6f:
                warning("Attempt to write to VMC_ARG0 addr=%08X, val=%02X",addr, val);
                break;
            case 0x70 ... 0x73:
                warning("Attempt to write to VMC_ARG1 addr=%08X, val=%02X",addr, val);
                break;
            case 0x74 ... 0x77:
                warning("Attempt to write to VMC_SNOOP_ARG0 addr=%08X, val=%02X",addr, val);
                break;
            case 0x78 ... 0x7b:
                warning("Attempt to write to VMC_SNOOP_ARG1 addr=%08X, val=%02X",addr, val);
                break;
            case 0xac ... 0xaf:
                warning("Attempt to write to BUF0_CAP_ODD_OFFSET addr=%08X, val=%02X",addr, val);
                break;
            case 0xb0 ... 0xb3:
                warning("Attempt to write to BUF1_CAP_ODD_OFFSET addr=%08X, val=%02X",addr, val);
                break;
            case 0xc0 ... 0xff:
                warning("Attempt to write to VMC_STRM_DATA addr=%08X, val=%02X",addr, val);
                break;
            case 0x140 ... 0x143:
                warning("Attempt to write to HW_DEBUG addr=%08X, val=%02X",addr, val);
                break;
            #endif
            case 0x80 ... 0x83:
                WRITE8(addr, atirage->buf_offset[0], val);
                break;
            case 0x8c ... 0x8f:
                WRITE8(addr, atirage->buf_pitch[0], val);
                break;
            case 0x98 ... 0x9b:
                WRITE8(addr, atirage->buf_offset[1], val);
                break;
            case 0xa4 ... 0xa7:
                WRITE8(addr, atirage->buf_pitch[1], val);
                break;
            default:
                break;
        }

        // pclog( "atirage_ext_writeb: addr=%04x val=%02x\n", addr, val);
    } else if (addr & 0x300) {
        atirage_queue(atirage, addr & 0x3ff, val, FIFO_WRITE_BYTE);
    } else {
        // pclog( "atirage_ext_writeb: addr=%04x val=%02x\n", addr & 0x3ff, val);
        switch (addr & 0x3ff) {
            case 0x00 ... 0x03:
                WRITE8(addr, atirage->crtc_h_total_disp, val);
                svga_recalctimings(&atirage->svga);
                svga->fullchange = svga->monitor->mon_changeframecount;
                break;
            case 0x04 ... 0x07:
                WRITE8(addr, atirage->crtc_h_sync_strt_wid, val);
                svga_recalctimings(&atirage->svga);
                svga->fullchange = svga->monitor->mon_changeframecount;
                break;
            case 0x08 ... 0x0b:
                WRITE8(addr, atirage->crtc_v_total_disp, val);
                svga_recalctimings(&atirage->svga);
                svga->fullchange = svga->monitor->mon_changeframecount;
                break;
            case 0x0c ... 0x0f:
                WRITE8(addr, atirage->crtc_v_sync_strt_wid, val);
                svga_recalctimings(&atirage->svga);
                svga->fullchange = svga->monitor->mon_changeframecount;
                break;
            case 0x14 ... 0x17:
                WRITE8(addr, atirage->crtc_off_pitch, val);
                svga_recalctimings(&atirage->svga);
                svga->fullchange = svga->monitor->mon_changeframecount;
                break;
            case 0x18:
                atirage->crtc_int_cntl = (atirage->crtc_int_cntl & 0x75) | (val & ~0x75);
                if (val & 4)
                    atirage->crtc_int_cntl &= ~4;
                atirage_update_irqs(atirage);
                break;
            case 0x1c ... 0x1f:
                WRITE8(addr, atirage->crtc_gen_cntl, val);
                if (((atirage->crtc_gen_cntl >> 24) & 3) == 3)
                    svga->fb_only = 1;
                else
                    svga->fb_only = 0;
                svga->dpms = !!(atirage->crtc_gen_cntl & 0x0c);
                svga_recalctimings(&atirage->svga);
                svga->fullchange = svga->monitor->mon_changeframecount;
                break;
            case 0x20 ... 0x23:
                WRITE8(addr, atirage->dsp_config, val);
                break;
            case 0x24 ... 0x27:
                WRITE8(addr, atirage->dsp_on_off, val);
                break;
            case 0x4c ... 0x4f:
                WRITE8(addr, atirage->vga_dsp_config, val);
                break;
            case 0x50 ... 0x53:
                WRITE8(addr, atirage->vga_dsp_on_off, val);
                break;
            case 0x40 ... 0x43:
                WRITE8(addr, atirage->ovr_clr, val);
                break;
            case 0x44 ... 0x47:
                WRITE8(addr, atirage->ovr_wid_left_right, val);
                break;
            case 0x48 ... 0x4b:
                WRITE8(addr, atirage->ovr_wid_top_bottom, val);
                break;
            case 0x60 ... 0x63:
                WRITE8(addr, atirage->cur_clr0, val);
                break;
            case 0x64 ... 0x67:
                WRITE8(addr, atirage->cur_clr1, val);
                break;
            case 0x68 ... 0x6b:
                WRITE8(addr, atirage->cur_offset, val);
                svga->hwcursor.addr = (atirage->cur_offset & 0xfffff) << 3;
                break;
            case 0x6c ... 0x6f:
                WRITE8(addr, atirage->cur_horz_vert_posn, val);
                svga->hwcursor.x = atirage->cur_horz_vert_posn & 0x7ff;
                svga->hwcursor.y = (atirage->cur_horz_vert_posn >> 16) & 0x7ff;
                break;
            case 0x70 ... 0x73:
                WRITE8(addr, atirage->cur_horz_vert_off, val);
                svga->hwcursor.xoff = atirage->cur_horz_vert_off & 0x3f;
                svga->hwcursor.yoff = (atirage->cur_horz_vert_off >> 16) & 0x3f;
                break;
            case 0x78 ... 0x7b:
                if (atirage->type == MACH64_GTB) {
                    WRITE8(addr, atirage->gp_io, val);
                    {
                        i2c_gpio_set(atirage->i2c_tv, !!(atirage->gp_io & (1 << 11)) || !(atirage->gp_io & (1 << (11 + 16))), !!(atirage->gp_io & (1 << 4)) || !(atirage->gp_io & (1 << (4 + 16))));
                        i2c_gpio_set(atirage->i2c, !!(atirage->gp_io & (1 << 13)) || !(atirage->gp_io & (1 << (13 + 16))), !!(atirage->gp_io & (1 << 12)) || !(atirage->gp_io & (1 << (12 + 16))));
                    }
                }
                break;
            case 0x80 ... 0x83:
                WRITE8(addr, atirage->scratch_reg0, val);
                break;
            case 0x84 ... 0x87:
                WRITE8(addr, atirage->scratch_reg1, val);
                break;
            case 0x90 ... 0x93:
                WRITE8(addr, atirage->clock_cntl, val);
                pll_write(atirage, addr, val);
                ics2595_setclock(svga->clock_gen, atirage->pll_freq[atirage->clock_cntl & 3]);
                svga_recalctimings(&atirage->svga);
                break;
            case 0xa0 ... 0xa3:
                #ifdef USE_RAGE_BUS_CNTL
                WRITE8(addr, atirage->bus_cntl, val);
                #endif
                break;
            case 0xb0 ... 0xb3:
                WRITE8(addr, atirage->mem_cntl, val);
                break;
            // optimise
            case 0xb4:
                atirage->bank_w[0] = val << 15; // *32768
                // pclog( "atirage : write bank A0000-A7FFF set to %08X\n", atirage->bank_w[0]);
                break;
            case 0xb6:
                atirage->bank_w[1] = val << 15; // *32768
                // pclog( "atirage : write bank A8000-AFFFF set to %08X\n", atirage->bank_w[1]);
                break;
            case 0xb8:
                atirage->bank_r[0] = val << 15; // *32768
                // pclog( "atirage :  read bank A0000-A7FFF set to %08X\n", atirage->bank_r[0]);
                break;
            case 0xba:
                atirage->bank_r[1] = val << 15; // *32768
                // pclog( "atirage :  read bank A8000-AFFFF set to %08X\n", atirage->bank_r[1]);
                break;
            case 0xc0 ... 0xc3:
                uint16_t port_list[4] = { 0x3c8, 0x3c9, 0x3c6, 0x3c7 }; 
                svga_out(port_list[addr & 3], val, svga);
                break;
            case 0xc4 ... 0xc7:
                WRITE8(addr, atirage->dac_cntl, val);
                // pclog( "Ext RAMDAC TYPE write=%x, bit set=%03x.\n", addr & 0x3ff, atirage->dac_cntl & 0x100);
                if ((addr & 3) >= 1) {
                    svga_set_ramdac_type(svga, !!(atirage->dac_cntl & 0x100));
                }
                if (atirage->type != MACH64_GTB)
                    i2c_gpio_set(atirage->i2c, !(atirage->dac_cntl & 0x20000000) || (atirage->dac_cntl & 0x04000000), !(atirage->dac_cntl & 0x10000000) || (atirage->dac_cntl & 0x02000000));
                break;
            case 0xd0 ... 0xd3:
                WRITE8(addr, atirage->gen_test_cntl, val);
                ati_eeprom_write(&atirage->eeprom, atirage->gen_test_cntl & 0x10, atirage->gen_test_cntl & 2, atirage->gen_test_cntl & 1);
                atirage->gen_test_cntl  = (atirage->gen_test_cntl & ~8) | (ati_eeprom_read(&atirage->eeprom) ? 8 : 0);
                svga->hwcursor.ena = !!(atirage->gen_test_cntl & 0x80);
                break;
            case 0xdc ... 0xdf:
                WRITE8(addr, atirage->config_cntl, val);
                atirage_updatemapping(atirage);
                break;
            case 0xe4 ... 0xe7:
                WRITE8(addr, atirage->config_stat0, val);
                break;
            default:
                break;
        }
    }
}

void
atirage_ext_writew(uint32_t addr, uint16_t val, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;
    svga_t    *svga    = &atirage->svga;

    if ((addr >= 0x000a0000) && (addr < 0x000bf800)) {
        svga->mapping.write_w(addr, val, svga->mapping.priv);
        return;
    }
    if (!((addr < 0x000a0000) || ((addr >= 0x000bf800) && (addr <= 0x000bffff)) || (addr >= 0x00100000)))
        return;

    // pclog( "atirage_ext_writew : addr %08X val %04X\n", addr, val);
    if (!(addr & 0x400)) {
        // pclog( "atirage_ext_writew: addr=%04x val=%04x\n", addr, val);
        atirage_ext_writeb(addr, val, priv);
        atirage_ext_writeb(addr + 1, val >> 8, priv);
    } else if (addr & 0x300) {
        atirage_queue(atirage, addr & 0x3fe, val, FIFO_WRITE_WORD);
    } else {
        atirage_ext_writeb(addr, val, priv);
        atirage_ext_writeb(addr + 1, val >> 8, priv);
    }
}
void
atirage_ext_writel(uint32_t addr, uint32_t val, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;
    svga_t    *svga    = &atirage->svga;

    if ((addr >= 0x000a0000) && (addr < 0x000bf800)) {
        svga->mapping.write_l(addr, val, svga->mapping.priv);
        return;
    }
    if (!((addr < 0x000a0000) || ((addr >= 0x000bf800) && (addr <= 0x000bffff)) || (addr >= 0x00100000)))
        return;

    if ((addr & 0x3c0) != 0x200)
        ; // pclog( "atirage_ext_writel : addr %08X val %08X\n", addr, val);
    if (!(addr & 0x400)) {
        // pclog( "atirage_ext_writel: addr=%04x val=%08x\n", addr, val);

        atirage_ext_writew(addr, val, priv);
        atirage_ext_writew(addr + 2, val >> 16, priv);
    } else if (addr & 0x300) {
        atirage_queue(atirage, addr & 0x3fc, val, FIFO_WRITE_DWORD);
    } else {
        atirage_ext_writew(addr, val, priv);
        atirage_ext_writew(addr + 2, val >> 16, priv);
    }
}

uint8_t
atirage_ext_inb(uint16_t port, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;
    svga_t   *svga   = &atirage->svga;
    uint8_t   ret    = 0x00; // default value based on invaid port

    // Code is written with the assumption that IO_BASE = 2cc. so we can rewrite it liek this
    if (atirage->io_base == ATIRAGE_IO_BASE_1CC)
        port -= 0x120;
    else if (atirage->io_base == ATIRAGE_IO_BASE_1C8)
        port -= 0x124;

    uint8_t port_high = (port >> 8) & 0xFE; // we only care about the upper 5 bits
    uint8_t port_low = port & 0xFF;

    uint8_t lane = port & 3;

    // the value to or the final address for write into space
    uint8_t addr_or_value = 0;

    // we only care about (ec...ef)
    if ((port_low >= 0xEC) && (port_low <= 0xEF))
    {
        // exclude everything we don't want
        switch (port_high)
        {
            // some special cases
            case 0x56: // 56ec-56ef
            case 0x5a: 

                addr_or_value = 0xB4;

                if (port_high == 0x5a)
                    addr_or_value = 0xb8;    

                if ((port_low == 0xED) || (port_low == 0xEF))
                    ret = 0x00;
                else if (port_low == 0xEC)                 
                    ret = atirage_ext_readb(0x400 | addr_or_value, priv);
                else
                    ret = atirage_ext_readb(0x400 | (addr_or_value + 2), priv);
                break; 
            case 0x5e: // 5eec-5eef
                uint16_t port_list[4] = { 0x3c8, 0x3c9, 0x3c6, 0x3c7 }; 
                ret = svga_in(port_list[lane], svga);
                break;
            case 0x6a: // 6eec-6eef
                atirage->config_cntl = (atirage->config_cntl & ~0x3ff0) | ((atirage->linear_base >> 22) << 4);
                READ8(port, atirage->config_cntl);
                break;
            default:  // general case
                // there must be a more rational rule here
                if (port_high <= 0x1E)
                    addr_or_value = port_high - 2;
                else if ((port_high >= 0x22 && port_high <= 0x2A))
                    addr_or_value = port_high + 0x1E; // 0x40 - 0x48
                else if ((port_high >= 0x2E && port_high <= 0x3E))
                    addr_or_value = port_high + 0x32;
                else if ((port_high >= 0x42 && port_high <= 0x46))
                    addr_or_value = port_high + 0x3E;
                else if (port_high == 0x4A)
                    addr_or_value = 0x90;
                else if (port_high == 0x4E) {
                    #ifdef USE_RAGE_BUS_CNTL
                    addr_or_value = 0xa0;
                    #endif
                }
                else if (port_high == 0x52)
                    addr_or_value = 0xb0;
                else if (port_high == 0x62)
                    addr_or_value = 0xc4;
                else if (port_high == 0x66)
                    addr_or_value = 0xd0;
                else if ((port_high >= 0x6e && port_high <= 0x72))
                    addr_or_value = port_high + 0x72;
                else if (port_high == 0x7e)
                    addr_or_value = 0x00; // must be 0

                ret = atirage_ext_readb(0x400 | addr_or_value | (lane), priv);
                break;
        }
    }

    // pclog( "atirage_ext_inb : port %04X ret %02X\n", port, ret);
    return ret;
}
uint16_t
atirage_ext_inw(uint16_t port, void *priv)
{
    uint16_t ret;

    ret = atirage_ext_inb(port, priv);
    ret |= (atirage_ext_inb(port + 1, priv) << 8);

    // pclog( "atirage_ext_inw : port %04X ret %04X\n", port, ret);
    return ret;
}

uint32_t
atirage_ext_inl(uint16_t port, void *priv)
{
    uint32_t ret;

    ret = atirage_ext_inw(port, priv);
    ret |= (atirage_ext_inw(port + 2, priv) << 16);

    // pclog( "atirage_ext_inl : port %04X ret %08X\n", port, ret);
    return ret;
}

void
atirage_ext_outb(uint16_t port, uint8_t val, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;
    svga_t *svga = &atirage->svga;

    // Code is written with the assumption that IO_BASE = 2cc. so we can rewrite it for other I/O bases like this
    if (atirage->io_base == ATIRAGE_IO_BASE_1CC)
        port -= 0x120;
    else if (atirage->io_base == ATIRAGE_IO_BASE_1C8)
        port -= 0x124;

    uint8_t port_high = (port >> 8) & 0xFE; // we only care about the upper 5 bits
    uint8_t port_low = port & 0xFF;

    // the value to or the final address for write into

    uint8_t addr_or_value = 0;

    // we only care about (ec...ef)
    if ((port_low >= 0xEC) && (port_low <= 0xEF))
    {
        switch (port_high)
        {
             case 0x56: // 56ec-56ef
                if ((port_low == 0xED) || (port_low == 0xEF))
                    break;
                if (port_low == 0xEC)                 
                    atirage_ext_writeb(0x400 | 0xb4, val, priv);
                else
                    atirage_ext_writeb(0x400 | 0xb6, val, priv);
                break; 
            case 0x5a: // 5aec-5aef
                if ((port_low == 0xED) || (port_low == 0xEF))
                    break;
                if (port_low == 0xEC)                 
                    atirage_ext_writeb(0x400 | 0xb8, val, priv);
                else
                    atirage_ext_writeb(0x400 | 0xba, val, priv);
                break; 
            case 0x5e: // 5eec-5eef
                uint16_t port_list[4] = { 0x3c8, 0x3c9, 0x3c6, 0x3c7 }; 
                svga_out(port_list[port & 3], val, svga);
                break;
            case 0x6a: // 6eec-6eef
                WRITE8(port, atirage->config_cntl, val);
                atirage_updatemapping(atirage);
                break;
            default:
                 // there must be a more rational rule here
                if (port_high <= 0x1E)
                    addr_or_value = port_high - 2;
                else if ((port_high >= 0x22) && (port_high <= 0x2A))
                    addr_or_value = port_high + 0x1E; // 0x40 - 0x48
                else if ((port_high >= 0x2E) && (port_high <= 0x3E))
                    addr_or_value = port_high + 0x32;
                else if ((port_high >= 0x42) && (port_high <= 0x46))
                    addr_or_value = port_high + 0x3E;
                else if (port_high == 0x4A) 
                    addr_or_value = 0x90;
                else if (port_high == 0x4E) {
                    #ifdef USE_RAGE_BUS_CNTL
                    addr_or_value = 0xa0;
                    #endif
                }
                else if (port_high == 0x52)
                    addr_or_value = 0xb0;
                else if (port_high == 0x62)
                    addr_or_value = 0xc4;
                else if (port_high == 0x66)
                    addr_or_value = 0xd0;
                else if ((port_high >= 0x6e) && (port_high <= 0x72))
                    addr_or_value = port_high + 0x72;
                else if (port_high == 0x7e)
                    addr_or_value = 0x00; // must be 0

                atirage_ext_writeb(0x400 | addr_or_value | (port & 3), val, priv);

                break;
        }
    }

    // pclog( "atirage_ext_outb : port %04X val %02X\n", port, val);
}
void
atirage_ext_outw(uint16_t port, uint16_t val, void *priv)
{
    // pclog( "atirage_ext_outw : port %04X val %04X\n", port, val);
    atirage_ext_outb(port, val, priv);
    atirage_ext_outb(port + 1, val >> 8, priv);
}
void
atirage_ext_outl(uint16_t port, uint32_t val, void *priv)
{
    // pclog( "atirage_ext_outl : port %04X val %08X\n", port, val);
    atirage_ext_outw(port, val, priv);
    atirage_ext_outw(port + 2, val >> 16, priv);
}

static uint8_t
atirage_block_inb(uint16_t port, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;
    uint8_t   ret;

    ret = atirage_ext_readb(0x400 | (port & 0x3ff), atirage);
    // pclog( "atirage_block_inb : port %04X ret %02X\n", port, ret);
    return ret;
}
static uint16_t
atirage_block_inw(uint16_t port, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;
    uint16_t  ret;

    ret = atirage_ext_readw(0x400 | (port & 0x3ff), atirage);
    // pclog( "atirage_block_inw : port %04X ret %04X\n", port, ret);
    return ret;
}
static uint32_t
atirage_block_inl(uint16_t port, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;
    uint32_t  ret;

    ret = atirage_ext_readl(0x400 | (port & 0x3ff), atirage);
    // pclog( "atirage_block_inl : port %04X ret %08X\n", port, ret);
    return ret;
}

static void
atirage_block_outb(uint16_t port, uint8_t val, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;

    // pclog( "atirage_block_outb : port %04X val %02X\n ", port, val);
    atirage_ext_writeb(0x400 | (port & 0x3ff), val, atirage);
}
static void
atirage_block_outw(uint16_t port, uint16_t val, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;

    // pclog( "atirage_block_outw : port %04X val %04X\n ", port, val);
    atirage_ext_writew(0x400 | (port & 0x3ff), val, atirage);
}
static void
atirage_block_outl(uint16_t port, uint32_t val, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;

    // pclog( "atirage_block_outl : port %04X val %08X\n ", port, val);
    atirage_ext_writel(0x400 | (port & 0x3ff), val, atirage);
}

static uint32_t
atirage_decode_addr(atirage_t *atirage, uint32_t addr, int write)
{
    const svga_t *svga            = &atirage->svga;
    const int     memory_map_mode = (svga->gdcreg[6] >> 2) & 3;

    addr &= 0x1ffff;

    switch (memory_map_mode) {
        case 0:
            break;
        case 1:
            if (addr >= 0x10000)
                return 0xffffffff;
            break;
        case 2:
            addr -= 0x10000;
            if (addr >= 0x8000)
                return 0xffffffff;
            break;
        default:
        case 3:
            addr -= 0x18000;
            if (addr >= 0x8000)
                return 0xffffffff;
            break;
    }

    if (write)
        addr = (addr & 0x7fff) + atirage->bank_w[(addr >> 15) & 1];
    else
        addr = (addr & 0x7fff) + atirage->bank_r[(addr >> 15) & 1];

    return addr;
}

void
atirage_write(uint32_t addr, uint8_t val, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;
    svga_t   *svga   = &atirage->svga;
    addr = atirage_decode_addr(atirage, addr, 1);
    if (addr != 0xffffffff)
        svga_write_linear(addr, val, svga);
}
void
atirage_writew(uint32_t addr, uint16_t val, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;
    svga_t   *svga   = &atirage->svga;
    addr = atirage_decode_addr(atirage, addr, 1);
    if (addr != 0xffffffff)
        svga_writew_linear(addr, val, svga);
}
void
atirage_writel(uint32_t addr, uint32_t val, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;
    svga_t   *svga   = &atirage->svga;
    addr = atirage_decode_addr(atirage, addr, 1);
    if (addr != 0xffffffff)
        svga_writel_linear(addr, val, svga);
}

uint8_t
atirage_read(uint32_t addr, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;
    svga_t   *svga   = &atirage->svga;
    uint8_t   ret = 0xff;
    addr = atirage_decode_addr(atirage, addr, 0);
    if (addr != 0xffffffff)
        ret = svga_read_linear(addr, svga);
    return ret;
}
uint16_t
atirage_readw(uint32_t addr, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;
    svga_t   *svga   = &atirage->svga;
    uint16_t  ret = 0xffff;
    addr = atirage_decode_addr(atirage, addr, 0);
    if (addr != 0xffffffff)
        ret = svga_readw_linear(addr, svga);
    return ret;
}
uint32_t
atirage_readl(uint32_t addr, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;
    svga_t   *svga   = &atirage->svga;
    uint32_t  ret = 0xffffffff;
    addr = atirage_decode_addr(atirage, addr, 0);
    if (addr != 0xffffffff)
        ret = svga_readl_linear(addr, svga);
    return ret;
}

uint32_t
atirage_conv_16to32(svga_t* svga, uint16_t color, uint8_t bpp)
{
    uint32_t ret = 0x00000000;

    if (svga->lut_map) {
        if (bpp == 15) {
            uint8_t b = getcolr(svga->pallook[(color & 0x1f) << 3]);
            uint8_t g = getcolg(svga->pallook[(color & 0x3e0) >> 2]);
            uint8_t r = getcolb(svga->pallook[(color & 0x7c00) >> 7]);
            ret = (video_15to32[color] & 0xFF000000) | makecol(r, g, b);
        } else {
            uint8_t b = getcolr(svga->pallook[(color & 0x1f) << 3]);
            uint8_t g = getcolg(svga->pallook[(color & 0x7e0) >> 3]);
            uint8_t r = getcolb(svga->pallook[(color & 0xf800) >> 8]);
            ret = (video_16to32[color] & 0xFF000000) | makecol(r, g, b);
        }
    } else
        ret = (bpp == 15) ? video_15to32[color] : video_16to32[color];

    return ret;
}

void
atirage_int_hwcursor_draw(svga_t *svga, int displine)
{
    const atirage_t          *atirage = (atirage_t *) svga->priv;
    int                      comb;
    int                      offset;
    int                      x_pos;
    int                      y_pos;
    int                      shift = 0;
    uint16_t                 dat;
    uint32_t                 col0 = makecol32((atirage->cur_clr0 >> 24) & 0xff, (atirage->cur_clr0 >> 16) & 0xff, (atirage->cur_clr0 >> 8) & 0xff);
    uint32_t                 col1 = makecol32((atirage->cur_clr1 >> 24) & 0xff, (atirage->cur_clr1 >> 16) & 0xff, (atirage->cur_clr1 >> 8) & 0xff);
    uint32_t                *p;

    offset = svga->hwcursor_latch.x - svga->hwcursor_latch.xoff;
    if (svga->packed_4bpp)
        shift = 1;

    for (int x = 0; x < svga->hwcursor_latch.cur_xsize; x += (8 >> shift)) {
        if (shift) {
            dat = svga->vram[(svga->hwcursor_latch.addr) & svga->vram_mask] & 0x0f;
            dat |= (svga->vram[(svga->hwcursor_latch.addr + 1) & svga->vram_mask] << 4);
            dat |= (svga->vram[(svga->hwcursor_latch.addr + 2) & svga->vram_mask] << 8);
            dat |= (svga->vram[(svga->hwcursor_latch.addr + 3) & svga->vram_mask] << 12);
        } else {
            dat = svga->vram[svga->hwcursor_latch.addr & svga->vram_mask];
            dat |= (svga->vram[(svga->hwcursor_latch.addr + 1) & svga->vram_mask] << 8);
        }
        for (int xx = 0; xx < (8 >> shift); xx++) {
            comb = (dat >> (xx << 1)) & 0x03;

            y_pos = displine;
            x_pos = offset + svga->x_add;
            p     = buffer32->line[y_pos];

            if (offset >= svga->hwcursor_latch.x) {
                switch (comb) {
                    case 0:
                        p[x_pos] = col0;
                        break;
                    case 1:
                        p[x_pos] = col1;
                        break;
                    case 3:
                        p[x_pos] ^= 0xffffff;
                        break;
                    default:
                        break;
                }
            }
            offset++;
        }
        svga->hwcursor_latch.addr += 2;
    }
}


static void
atirage_io_unmap(atirage_t *atirage)
{
    uint16_t io_base = ATIRAGE_IO_BASE_2EC;

    switch (atirage->io_base) {
        default:
        case 0:
            io_base = ATIRAGE_IO_BASE_2EC;
            break;
        case 1:
            io_base = ATIRAGE_IO_BASE_1CC;
            break;
        case 2:
            io_base = ATIRAGE_IO_BASE_1C8;
            break;
        case 3:
            fatal("Attempting to use the reserved value for I/O Base\n");
            return;
    }

    io_removehandler(0x03c0, 0x0020, atirage_in, NULL, NULL, atirage_out, NULL, NULL, atirage);

    for (uint8_t c = 0; c < 32; c++) // *0x400
        io_removehandler((c << 10) + io_base, 0x0004, atirage_ext_inb, atirage_ext_inw, atirage_ext_inl, atirage_ext_outb, atirage_ext_outw, atirage_ext_outl, atirage);

    io_removehandler(0x01ce, 0x0002, atirage_in, NULL, NULL, atirage_out, NULL, NULL, atirage);

    if (atirage->block_decoded_io && atirage->block_decoded_io < 0x10000)
        io_removehandler(atirage->block_decoded_io, 0x0100, atirage_block_inb, atirage_block_inw, atirage_block_inl, atirage_block_outb, atirage_block_outw, atirage_block_outl, atirage);
}

static void
atirage_io_map(atirage_t *atirage)
{
    uint16_t io_base = ATIRAGE_IO_BASE_2EC;

    atirage_io_unmap(atirage);

    switch (atirage->io_base) {
        default:
        case 0:
            io_base = ATIRAGE_IO_BASE_2EC;
            break;
        case 1:
            io_base = ATIRAGE_IO_BASE_1CC;
            break;
        case 2:
            io_base = ATIRAGE_IO_BASE_1C8;
            break;
        case 3:
            fatal("Attempting to use the reserved value for I/O Base\n");
            return;
    }

    io_sethandler(0x03c0, 0x0020, atirage_in, NULL, NULL, atirage_out, NULL, NULL, atirage);

    if (!atirage->use_block_decoded_io) {

        for (uint8_t c = 0; c < 32; c++) // *0x400
            io_sethandler((c << 10) + io_base, 0x0004, atirage_ext_inb, atirage_ext_inw, atirage_ext_inl, atirage_ext_outb, atirage_ext_outw, atirage_ext_outl, atirage);
    }

    io_sethandler(0x01ce, 0x0002, atirage_in, NULL, NULL, atirage_out, NULL, NULL, atirage);

    if (atirage->use_block_decoded_io && atirage->block_decoded_io && atirage->block_decoded_io < 0x10000)
        io_sethandler(atirage->block_decoded_io, 0x0100, atirage_block_inb, atirage_block_inw, atirage_block_inl, atirage_block_outb, atirage_block_outw, atirage_block_outl, atirage);
}

static uint8_t
atirage_read_linear(uint32_t addr, void *priv)
{
    const svga_t *svga = (svga_t *) priv;

    cycles -= svga->monitor->mon_video_timing_read_b;

    addr &= svga->decode_mask;
    if (addr >= svga->vram_max)
        return 0xff;

    return svga->vram[addr & svga->vram_mask];
}

static uint16_t
atirage_readw_linear(uint32_t addr, void *priv)
{
    svga_t *svga = (svga_t *) priv;

    cycles -= svga->monitor->mon_video_timing_read_w;

    addr &= svga->decode_mask;
    if (addr >= svga->vram_max)
        return 0xffff;

    return *(uint16_t *) &svga->vram[addr & svga->vram_mask];
}

static uint32_t
atirage_readl_linear(uint32_t addr, void *priv)
{
    svga_t *svga = (svga_t *) priv;

    cycles -= svga->monitor->mon_video_timing_read_l;

    addr &= svga->decode_mask;
    if (addr >= svga->vram_max)
        return 0xffffffff;

    return *(uint32_t *) &svga->vram[addr & svga->vram_mask];
}

static void
atirage_write_linear(uint32_t addr, uint8_t val, void *priv)
{
    svga_t *svga = (svga_t *) priv;
    cycles -= svga->monitor->mon_video_timing_write_b;

    addr &= svga->decode_mask;
    if (addr >= svga->vram_max)
        return;
    addr &= svga->vram_mask;
    svga->changedvram[addr >> 12] = svga->monitor->mon_changeframecount;
    svga->vram[addr]              = val;
}

static void
atirage_writew_linear(uint32_t addr, uint16_t val, void *priv)
{
    svga_t *svga = (svga_t *) priv;

    cycles -= svga->monitor->mon_video_timing_write_w;

    addr &= svga->decode_mask;
    if (addr >= svga->vram_max)
        return;
    addr &= svga->vram_mask;
    svga->changedvram[addr >> 12]   = svga->monitor->mon_changeframecount;
    *(uint16_t *) &svga->vram[addr] = val;
}

static void
atirage_writel_linear(uint32_t addr, uint32_t val, void *priv)
{
    svga_t   *svga   = (svga_t *) priv;
    atirage_t *atirage = (atirage_t *) svga->priv;

    cycles -= svga->monitor->mon_video_timing_write_l;

    if (((atirage->scaler_yuv_aper >> 4) & 0xc) && !!(addr & 0x800000) == !(atirage->scaler_yuv_aper & 0x20)) {
        uint32_t offset_from_base = addr & 0x7FFFFF;
        if (addr & 0x800000) 
            bswap32s(&val);
        if (((atirage->scaler_yuv_aper >> 4) & 0xc) == 0x4) { // Y plane
            offset_from_base <<= 1;
            svga->vram[offset_from_base & svga->vram_mask] = (val & 0xFF);
            svga->vram[(offset_from_base + 1) & svga->vram_mask] = ((val >> 8) & 0xFF);
            svga->vram[(offset_from_base + 4) & svga->vram_mask] = ((val >> 16) & 0xFF);
            svga->vram[(offset_from_base + 5) & svga->vram_mask] = ((val >> 24) & 0xFF);
        }
        else if (((atirage->scaler_yuv_aper >> 4) & 0xc) == 0x8 || ((atirage->scaler_yuv_aper >> 4) & 0xc) == 0xc) {
            offset_from_base <<= 2;
            if (((atirage->scaler_yuv_aper >> 4) & 0xc) == 0x8) { // U plane
                svga->vram[(offset_from_base + 3) & svga->vram_mask] = (val & 0xFF);
                svga->vram[(offset_from_base + 7) & svga->vram_mask] = ((val >> 8) & 0xFF);
                svga->vram[(offset_from_base + 11) & svga->vram_mask] = ((val >> 16) & 0xFF);
                svga->vram[(offset_from_base + 15) & svga->vram_mask] = ((val >> 24) & 0xFF);
            } else { // V plane
                svga->vram[(offset_from_base + 2) & svga->vram_mask] = (val & 0xFF);
                svga->vram[(offset_from_base + 6) & svga->vram_mask] = ((val >> 8) & 0xFF);
                svga->vram[(offset_from_base + 10) & svga->vram_mask] = ((val >> 16) & 0xFF);
                svga->vram[(offset_from_base + 14) & svga->vram_mask] = ((val >> 24) & 0xFF);
            }
        }
        return;
    }

    addr &= svga->decode_mask;
    if (addr >= svga->vram_max)
        return;
    addr &= svga->vram_mask;
    svga->changedvram[addr >> 12]   = svga->monitor->mon_changeframecount;
    *(uint32_t *) &svga->vram[addr] = val;
}

uint8_t
atirage_readb_be(uint32_t addr, void *priv)
{
    return atirage_read_linear(addr, priv);
}

uint16_t
atirage_readw_be(uint32_t addr, void *priv)
{
    return bswap16(atirage_readw_linear(addr, priv));
}

uint32_t
atirage_readl_be(uint32_t addr, void *priv)
{
    return bswap32(atirage_readl_linear(addr, priv));
}

void
atirage_writeb_be(uint32_t addr, uint8_t val, void *priv)
{
    return atirage_write_linear(addr, val, priv);
}

void
atirage_writew_be(uint32_t addr, uint16_t val, void *priv)
{
    return atirage_writew_linear(addr, bswap16(val), priv);
}

void
atirage_writel_be(uint32_t addr, uint32_t val, void *priv)
{
    return atirage_writel_linear(addr, bswap32(val), priv);
}

// PCI config space I/O read function
uint8_t
atirage_pci_read(UNUSED(int func), int addr, UNUSED(int len), void *priv)
{
    const atirage_t *atirage = (atirage_t *) priv;

    switch (addr) {
        case PCI_REG_VENDOR_ID_L:
            // pclog( "RAGE PCI read address 0x%02X returns 0x02\n",addr);
            return 0x02; /*ATi*/
        case PCI_REG_VENDOR_ID_H:
            // pclog( "RAGE PCI read address 0x%02X returns 0x10\n",addr);
            return 0x10;
        case PCI_REG_DEVICE_ID_L:
            // pclog( "RAGE PCI read address 0x%02X returns 0x%02X\n",addr,atirage->pci_id & 0xff);
            return atirage->pci_id & 0xff;
        case PCI_REG_DEVICE_ID_H:
            // pclog( "RAGE PCI read address 0x%02X returns 0x%02X\n",addr,atirage->pci_id >> 8);
            return atirage->pci_id >> 8;
        case PCI_REG_COMMAND:
            // pclog( "RAGE PCI read address 0x%02X returns command values\n",addr);
            return atirage->pci_regs[PCI_REG_COMMAND]; /*Respond to IO and memory accesses*/
        case PCI_REG_STATUS_H:
            // pclog( "RAGE PCI read address 0x%02X returns 0x02\n",addr);
            return 1 << 1; /*Medium DEVSEL timing*/
        case PCI_REG_REVISION: /*Revision ID*/
            // pclog( "RAGE PCI read address 0x%02X returns 0x40\n",addr);
            return 0x40;
        case PCI_REG_PROG_IF:
            // pclog( "RAGE PCI read address 0x%02X returns 0x00\n",addr);
            return 0; /*Programming interface*/
        case PCI_REG_SUBCLASS:
            // pclog( "RAGE PCI read address 0x%02X returns 0x01\n",addr);
            return 0x01; /*Supports VGA interface, XGA compatible*/
        case PCI_REG_CLASS:
            // pclog( "RAGE PCI read address 0x%02X returns 0x03\n",addr);
            return 0x03;
        case PCI_REG_BAR0_BYTE0:
            // pclog( "RAGE PCI read address 0x%02X returns 0x00\n",addr);
            return 0x00; /*Linear frame buffer address*/
        case PCI_REG_BAR0_BYTE1:
            // pclog( "RAGE PCI read address 0x%02X returns 0x00\n",addr);
            return 0x00;
        case PCI_REG_BAR0_BYTE2:
            // pclog( "RAGE PCI read address 0x%02X returns 0x%02X\n",addr,atirage->linear_base >> 16);
            return atirage->linear_base >> 16;
        case PCI_REG_BAR0_BYTE3:
            // pclog( "RAGE PCI read address 0x%02X returns 0x%02X\n",addr,atirage->linear_base >> 24);
            return atirage->linear_base >> 24;
        case PCI_REG_BAR1_BYTE0:
            // pclog( "RAGE PCI read address 0x%02X returns 0x01\n",addr);
            return 0x01; /*Block decoded IO address*/
        case PCI_REG_BAR1_BYTE1:
            // pclog( "RAGE PCI read address 0x%02X returns 0x%02X\n",addr,atirage->block_decoded_io >> 8);
            return atirage->block_decoded_io >> 8;
        case PCI_REG_BAR1_BYTE2:
            // pclog( "RAGE PCI read address 0x%02X returns 0x%02X\n",addr,atirage->block_decoded_io >> 16);
            return atirage->block_decoded_io >> 16;
        case PCI_REG_BAR1_BYTE3:
            // pclog( "RAGE PCI read address 0x%02X returns 0x%02X\n",addr,atirage->block_decoded_io >> 24);
            return atirage->block_decoded_io >> 24;
        case PCI_REG_ROM_BAR_BYTE0:
            // pclog( "RAGE PCI read address 0x%02X returns 0x%02X\n",addr,(atirage->on_board) ? 0 : (atirage->pci_regs[0x30] & 0x01));
            return (atirage->on_board) ? 0 : (atirage->pci_regs[0x30] & 0x01); /*BIOS ROM address*/
        case PCI_REG_ROM_BAR_BYTE1:
            // pclog( "RAGE PCI read address 0x%02X returns 0x00\n",addr);
            return 0x00;
        case PCI_REG_ROM_BAR_BYTE2:
            // pclog( "RAGE PCI read address 0x%02X returns 0x%02X\n",addr,(atirage->on_board) ? 0 : atirage->pci_regs[0x32]);
            return (atirage->on_board) ? 0 : atirage->pci_regs[0x32];
        case PCI_REG_ROM_BAR_BYTE3:
            // pclog( "RAGE PCI read address 0x%02X returns 0x%02X\n",addr,(atirage->on_board) ? 0 : atirage->pci_regs[0x33]);
            return (atirage->on_board) ? 0 : atirage->pci_regs[0x33];
        case PCI_REG_INT_LINE:
            // pclog( "RAGE PCI read address 0x%02X returns 0x%02X\n",addr,atirage->int_line);
            return atirage->int_line;
        case PCI_REG_INT_PIN:
            // pclog( "RAGE PCI read address 0x%02X returns 0x%02X\n",addr,PCI_INTA);
            return PCI_INTA;
        case ATIRAGE_PCI_IOCONFIG:
            // pclog( "RAGE PCI read address 0x%02X returns 0x%02X\n",addr,atirage->use_block_decoded_io | atirage->io_base);
            return atirage->use_block_decoded_io | atirage->io_base;
        default:
            // pclog( "RAGE PCI read address 0x%02X unknown\n",addr);
            break;
    }
    return 0;
}

// PCI config space I/O write function
void
atirage_pci_write(UNUSED(int func), int addr, UNUSED(int len), uint8_t val, void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;

    // Addresses that DON'T need to 
    bool dont_remap_io = (addr == PCI_REG_COMMAND // controls the mapping so we don't use the default behaviour  
    || addr == PCI_REG_BAR0_BYTE2
    || addr == PCI_REG_BAR0_BYTE3
    || (addr >= PCI_REG_ROM_BAR_BYTE0 || addr <= PCI_REG_ROM_BAR_BYTE3));

    if (!dont_remap_io
    && (atirage->pci_regs[PCI_REG_COMMAND] & PCI_COMMAND_IO))
    {
        atirage_io_unmap(atirage);
    }

    switch (addr) {
        case PCI_REG_COMMAND:
            atirage->pci_regs[PCI_REG_COMMAND] = val & 0x27;
            if (val & PCI_COMMAND_IO)
                atirage_io_map(atirage);
            else
                atirage_io_unmap(atirage);
            atirage_updatemapping(atirage);
            break;
        case PCI_REG_BAR0_BYTE2:
            val = 0;
            atirage->linear_base = (atirage->linear_base & 0xff000000) | ((val & 0x80) << 16);
            atirage_updatemapping(atirage);
            break;
        case PCI_REG_BAR0_BYTE3:
            atirage->linear_base = (atirage->linear_base & 0x800000) | (val << 24);
            atirage_updatemapping(atirage);
            break;
        case PCI_REG_BAR1_BYTE1:
            atirage->block_decoded_io = (atirage->block_decoded_io & 0xffff0000) | ((val & 0xff) << 8);
            break;
        case PCI_REG_BAR1_BYTE2:
            atirage->block_decoded_io = (atirage->block_decoded_io & 0xff00fc00) | (val << 16);
            break;
        case PCI_REG_BAR1_BYTE3:
            atirage->block_decoded_io = (atirage->block_decoded_io & 0x00fffc00) | (val << 24);
            break;
        case PCI_REG_ROM_BAR_BYTE0:
        case PCI_REG_ROM_BAR_BYTE2 ... PCI_REG_ROM_BAR_BYTE3:
            if (atirage->on_board) 
                return;
            atirage->pci_regs[addr] = val;
            if (atirage->pci_regs[PCI_REG_ROM_BAR_BYTE0] & 0x01) {
                uint32_t biosaddr = (atirage->pci_regs[0x32] << 16) | (atirage->pci_regs[0x33] << 24);
                // pclog( "Mach64 bios_rom enabled at %08x\n", biosaddr);
                mem_mapping_set_addr(&atirage->bios_rom.mapping, biosaddr, 0x8000);
            } else {
                // pclog( "Mach64 bios_rom disabled\n");
                mem_mapping_disable(&atirage->bios_rom.mapping);
            }
            return;
        case PCI_REG_INT_LINE:
            atirage->int_line = val;
            break;
        case ATIRAGE_PCI_IOCONFIG:
            atirage->io_base = val & 0x03;
            atirage->use_block_decoded_io = val & 0x04;
            break;
        default:
            break;
    }

    if (!dont_remap_io
    && (atirage->pci_regs[PCI_REG_COMMAND] & PCI_COMMAND_IO))
    {
        atirage_io_map(atirage);
    }
}

static void
atirage_disable_handlers(atirage_t *dev)
{
    atirage_io_unmap(dev);

    mem_mapping_disable(&dev->linear_mapping);
    mem_mapping_disable(&dev->linear_mapping_big_endian);
    mem_mapping_disable(&dev->mmio_mapping);
    mem_mapping_disable(&dev->mmio_linear_mapping);
    mem_mapping_disable(&dev->mmio_linear_mapping_2);
    mem_mapping_disable(&dev->svga.mapping);
    if (dev->pci && !dev->on_board)
        mem_mapping_disable(&dev->bios_rom.mapping);

    /* Save all the mappings and the timers because they are part of linked lists. */
    reset_state[dev->svga.monitor_index]->linear_mapping            = dev->linear_mapping;
    reset_state[dev->svga.monitor_index]->linear_mapping_big_endian = dev->linear_mapping_big_endian;
    reset_state[dev->svga.monitor_index]->mmio_mapping              = dev->mmio_mapping;
    reset_state[dev->svga.monitor_index]->mmio_linear_mapping       = dev->mmio_linear_mapping;
    reset_state[dev->svga.monitor_index]->mmio_linear_mapping_2     = dev->mmio_linear_mapping_2;
    reset_state[dev->svga.monitor_index]->svga.mapping              = dev->svga.mapping;
    reset_state[dev->svga.monitor_index]->bios_rom.mapping          = dev->bios_rom.mapping;

    reset_state[dev->svga.monitor_index]->svga.timer      = dev->svga.timer;
    reset_state[dev->svga.monitor_index]->svga.timer_8514 = dev->svga.timer_8514;
    reset_state[dev->svga.monitor_index]->svga.timer_xga  = dev->svga.timer_xga;
}

static void
atirage_reset(void *priv)
{
    atirage_t *dev = (atirage_t *) priv;

    if (reset_state[dev->svga.monitor_index] != NULL) {
        atirage_disable_handlers(dev);
        dev->blitter_busy                              = 0;
        dev->fifo_write_idx                            = 0;
        dev->fifo_read_idx                             = 0;
        reset_state[dev->svga.monitor_index]->eeprom   = dev->eeprom;
        reset_state[dev->svga.monitor_index]->pci_slot = dev->pci_slot;

        *dev = *reset_state[dev->svga.monitor_index];
        atirage_io_map(dev);
        memset(dev->svga.vram, 0, dev->svga.vram_max);
        memset(dev->svga.changedvram, 0, (dev->svga.vram_max >> 12) + 1);
        dev->svga.dpms = 1;
        svga_recalctimings(&dev->svga);
        dev->svga.dpms = 0;
    }
}

static void *
atirage_common_init(const device_t *info)
{
    svga_t   *svga;
    atirage_t *atirage = calloc(1, sizeof(atirage_t));
    reset_state[monitor_index_global] = calloc(1, sizeof(atirage_t));

    svga = &atirage->svga;

    atirage->bank_w[0] = 0x0000;
    atirage->bank_w[1] = 0x8000;
    atirage->bank_r[0] = 0x0000;
    atirage->bank_r[1] = 0x8000;

    atirage->type = info->local & 0xff;
    atirage->vram_size = 2; /* Will change this later for GTB */
    atirage->vram_mask = (atirage->vram_size << 20) - 1;

    atirage->io_base = ATIRAGE_IO_BASE_2EC;

    svga_init(info, svga, atirage, atirage->vram_size << 20,
                atirage_recalctimings,
                atirage_in, atirage_out,
                atirage_int_hwcursor_draw,
                atirage_overlay_draw);

    mem_mapping_add(&atirage->linear_mapping, 0, 0, atirage_read_linear, atirage_readw_linear, atirage_readl_linear, atirage_write_linear, atirage_writew_linear, atirage_writel_linear, NULL, MEM_MAPPING_EXTERNAL, svga);
    mem_mapping_add(&atirage->linear_mapping_big_endian, 0, 0, atirage_readb_be, atirage_readw_be, atirage_readl_be, atirage_writeb_be, atirage_writew_be, atirage_writel_be, NULL, MEM_MAPPING_EXTERNAL, svga);
    mem_mapping_add(&atirage->mmio_linear_mapping, 0, 0, atirage_ext_readb, atirage_ext_readw, atirage_ext_readl, atirage_ext_writeb, atirage_ext_writew, atirage_ext_writel, NULL, MEM_MAPPING_EXTERNAL, atirage);
    mem_mapping_add(&atirage->mmio_linear_mapping_2, 0, 0, atirage_ext_readb, atirage_ext_readw, atirage_ext_readl, atirage_ext_writeb, atirage_ext_writew, atirage_ext_writel, NULL, MEM_MAPPING_EXTERNAL, atirage);
    mem_mapping_add(&atirage->mmio_mapping, 0xbf000, 0x1000, atirage_ext_readb, atirage_ext_readw, atirage_ext_readl, atirage_ext_writeb, atirage_ext_writew, atirage_ext_writel, NULL, MEM_MAPPING_EXTERNAL, atirage);
    mem_mapping_disable(&atirage->mmio_mapping);

    atirage_io_map(atirage);

    if (info->flags & DEVICE_PCI)
        pci_add_card((info->local & ATIRAGE_FLAG_ONBOARD) ? PCI_ADD_VIDEO : PCI_ADD_NORMAL, atirage_pci_read, atirage_pci_write, atirage, &atirage->pci_slot);

    atirage->pci_regs[PCI_REG_COMMAND]       = 3;
    atirage->pci_regs[PCI_REG_ROM_BAR_BYTE0] = 0x00;
    atirage->pci_regs[PCI_REG_ROM_BAR_BYTE2] = 0x0c;
    atirage->pci_regs[PCI_REG_ROM_BAR_BYTE3] = 0x00;

    svga->clock_gen = device_add(&ics2595_device);

    svga->conv_16to32 = atirage_conv_16to32;

    atirage->dst_cntl = 3;

    atirage->thread_run = 1;
    atirage->wake_fifo_thread = thread_create_event();
    atirage->fifo_not_full_event = thread_create_event();
    atirage->fifo_thread = thread_create(atirage_fifo_thread, atirage);
    atirage->on_board = !!(info->local & ATIRAGE_FLAG_ONBOARD);

    atirage->i2c = i2c_gpio_init("ddc_atirage");
    atirage->i2c_tv = i2c_gpio_init("tv_atirage");
    atirage->ddc = ddc_init(i2c_gpio_get_bus(atirage->i2c));

    atirage->scaler_active = 0;

#ifdef DMA_BM
    atirage->dma.lock = thread_create_mutex();
#endif

    return atirage;
}

static void *
mach64gt_init(const device_t *info)
{
    atirage_t *atirage = atirage_common_init(info);
    svga_t   *svga   = &atirage->svga;

    svga->dac_hwcursor_draw = NULL;

    svga->hwcursor.cur_ysize = 64;
    svga->hwcursor.cur_xsize = 64;

    video_inform(VIDEO_FLAG_TYPE_SPECIAL, &timing_atirage);

    atirage->pci                  = 1;
    atirage->agp                  = 0;
    atirage->pci_id               = 0x4754;
    atirage->config_chip_id       = 0x40004754;
    atirage->dac_cntl             = 1 << 16; /*Internal 24-bit DAC*/
    atirage->config_stat0         = 2; /* 3D Rage cards only used EDO memory */
    atirage->use_block_decoded_io = 4;

    atirage->bus_cntl             = 0x800f00ff;

    atirage->trapezoid_debug      = device_get_config_int("trapezoid_debug");

    ati_eeprom_load(&atirage->eeprom, "mach64gt.nvr", 1);
    rom_init(&atirage->bios_rom, BIOS_ROMGT_PATH, 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);
    mem_mapping_disable(&atirage->bios_rom.mapping);

    svga->vblank_start = atirage_vblank_start;
    svga->adv_flags   |= FLAG_PANNING_ATI;

    *reset_state[monitor_index_global] = *atirage;
    return atirage;
}


int
mach64gt_available(void)
{
    return rom_present(BIOS_ROMGT_PATH);
}

void
atirage_close(void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;

#ifdef DMA_BM
    atirage->dma.state = 0;
#endif
    atirage->thread_run = 0;
    thread_set_event(atirage->wake_fifo_thread);
    thread_wait(atirage->fifo_thread);
    thread_destroy_event(atirage->fifo_not_full_event);
    thread_destroy_event(atirage->wake_fifo_thread);
#ifdef DMA_BM
    thread_close_mutex(atirage->dma.lock);
#endif

    svga_close(&atirage->svga);

    ddc_close(atirage->ddc);
    i2c_gpio_close(atirage->i2c);
    i2c_gpio_close(atirage->i2c_tv);

    free(reset_state[atirage->svga.monitor_index]);
    free(atirage);
}

void
atirage_speed_changed(void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;

    svga_recalctimings(&atirage->svga);
}

void
atirage_force_redraw(void *priv)
{
    atirage_t *atirage = (atirage_t *) priv;

    atirage->svga.fullchange = atirage->svga.monitor->mon_changeframecount;
}

// clang-format off

static const device_config_t mach64gt_config[] = {
    {
        .name           = "trapezoid_debug",
        .description    = "Debug trapezoids",
        .type           = CONFIG_BINARY,
        .default_string = NULL,
        .default_int    = 1,
        .file_filter    = NULL,
        .spinner        = { 0 },
        .selection      = { { 0 } },
        .bios           = { { 0 } }
    },
    { .name = "", .description = "", .type = CONFIG_END }
};
// clang-format on

const device_t mach64gt_device = {
    .name          = "ATI Mach64GT",
    .internal_name = "mach64gt",
    .flags         = DEVICE_PCI,
    .local         = MACH64_GT,
    .init          = mach64gt_init,
    .close         = atirage_close,
    .reset         = NULL,
    .available     = mach64gt_available,
    .speed_changed = atirage_speed_changed,
    .force_redraw  = atirage_force_redraw,
    .config        = mach64gt_config
};
