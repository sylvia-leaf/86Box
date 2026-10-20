# Windows Me fullscreen DirectDraw freeze

## Horizontal timing

Dxdiag's fullscreen test selects 640x480 RGB555 and sets CR25 to `0x10`.
The emulator incorrectly added 256 character clocks to horizontal total for
that bit. The SVGA core then calculated 2048 pixels of left overscan, placing
the picture outside the output buffer. The host retained the last white dxdiag
window even while the guest continued drawing the test box into both buffers.

CR25 bit 4 actually supplies horizontal blank end bit 6. Horizontal total,
display end and blank start extensions come from CR2D bits 0, 1 and 2.
The fix decodes those fields into the SVGA timing values and blank-end mask.
The horizontal extension definitions are also used by Nouveau:
https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/dispnv04/crtc.c

## Interrupt handling

The NV3 interrupt controller could lose a pending interrupt during the status
and acknowledgement sequences used by a guest driver:

- Reading PMC.INTR cleared PCI INTA without acknowledging any engine status.
- PFIFO, PTIMER and PDMA acknowledgements unconditionally cleared the shared
  line, including when PGRAPH's vertical blank interrupt remained pending.
- PGRAPH tested status and enable masks separately, so unrelated bits could
  produce a spurious interrupt.
- Disabling PMC interrupts did not update the line.

The fix keeps status reads passive and derives INTA from all pending, enabled
sources after acknowledgements and mask changes. It also includes the independent
INVALID subinterrupt route and software interrupt enable, and couples INVALID
status acknowledgement to its parent status.

NV3 routes vertical blank through PMC bit 24 and other PGRAPH events through
bit 12. The interrupt masks operate per bit; INVALID_EN provides an additional
route to bit 12. References:

- https://envytools.readthedocs.io/en/latest/hw/graph/riva/pgraph.html#interrupts
- https://envytools.readthedocs.io/en/latest/hw/bus/pmc.html#interrupts

## Regression check

```sh
python3 tests/video/vid_riva128_display_test.py
cmake --build build -j 6
```

The standalone test compiles the production register and interrupt functions,
mocking only the external PCI line, logging, GPU work callback and unrelated
embedded types. It exercises repeated vblank/status/ack cycles, simultaneous
engine interrupts, masking, INVALID acknowledgement, software interrupts and
the fullscreen horizontal timing register values.
The pre-fix source fails when the first status read deasserts INTA.

## Guest validation

A temporary copy of `~/pcbox/winme-nv3` was booted using the software renderer
under GDB. Dxdiag reported `nv3disp.drv` version `4.11.01.0348`, DirectX 7.1,
and enabled DirectDraw acceleration. The windowed test rendered its white box.
Fullscreen left a frozen white host image; guest VRAM and FIFO inspection
showed that drawing was still progressing, and `svga.left_overscan` was 2048.

With the interrupt fixes compiled in, suppressing the erroneous horizontal
total extension live and recalculating timings changed left overscan to zero.
Dxdiag returned to the desktop confirmation and reached the end-of-tests
dialog. The final timing implementation was then compiled and regression
tested. A complete fresh-boot run of that final binary was not performed.
The original VM image was not modified. This validation concerns the freeze,
not pixel-perfect rendering or Direct3D.
