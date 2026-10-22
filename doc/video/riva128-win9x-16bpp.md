# RIVA 128 Windows 9x 16bpp corruption

The failure reproduced with the Windows Me VM at `~/pcbox/winme-nv3` is a
PGRAPH channel-switch ordering bug. The first surface-format command executes
before the resource manager restores the channel, so the restore overwrites
RGB555 with a saved RGB888 format. Scanout remains RGB555, causing incorrect
colors and horizontally doubled pixels.

## Driver evidence

The supplied `riva128drivers/nv3rm.vxd` is a 32-bit LE executable. Its SHA-256 is
`d59785c4a264a3a2d46af6cbfeb75d00346f286ad4370a66f77751b1ba529790`.
Object 1 starts at file offset `0xac00`, has size `0x56d1c`, and uses 512-byte
pages. The following offsets are relative to object 1. Apply LE relocations
before following absolute pointers; plain objdump is sufficient for the
register accesses and structure offsets themselves.

- `0xa123` calls the display-information wrapper at `0x3f57c`, passing a
  50-byte buffer. That wrapper invokes VxD service `0x000a0014`.
- `0xa145..0xa184` selects 8bpp when display-information flag bit 3 is set;
  otherwise it copies the byte at buffer offset `0x1f` to chip-state `+0x230`.
- `0x17621` normalizes the depth: 15/16 become 16; 24/30/32 become 32;
  other values take the 8bpp branch.
- `0x7abe..0x7ae4` clears bit 12 of DAC register `0x680600` for depth 16,
  selecting RGB555.
- `0x1909b..0x19149` selects PGRAPH surface register values 6, 4, and 7 for
  depths 16, 8, and the remaining branch, respectively.

The **installed** RM captured from guest memory has a different layout. Its
live instructions access depth at chip-state `+0x234`, where the value is 16.
The byte at display-information buffer offset `0x1f` is also 16. Reading
`+0x230` instead produces zero; that was a misleading lead from applying the
in-tree binary's layout to the installed driver. The live MMIO-base member is
`+0x4572`, versus `+0x456e` in the supplied binary, confirming the layout
mismatch independently.

In the installed RM, object-relative `0x358c3..0x35935` restores surface 0:
it reads the saved surface object's format at `+0x33c`, replaces the low two
bits of MMIO `0x4006a8`, then sets validity bit 2. This is the path that
replaces the prematurely executed format command's result.

## Reproduction and observed ordering

Run a **copy** of the VM with the software renderer:

```sh
QT_QPA_PLATFORM=offscreen build/src/PCBox -P /tmp/pcbox-nv3-repro -R /home/bruh/code/pcbox/roms -L /tmp/nv3.log -N
```

Temporary instrumentation recorded PGRAPH submissions, surface and DAC writes,
and VRAM snapshots. It was removed from the final implementation.

Before the fix:

1. The incoming channel is 0; `CTX_USER` reports channel 127.
2. PGRAPH raises the context-switch interrupt **and executes** class `0x1c`,
   method `0x300`, parameter `0x01000000` (RGB555).
3. The RM restores surface 0 to format 3 and sets validity: register value 7.
4. Surface 1 is subsequently configured correctly. The final register is
   `0x67`: surface 0 is RGB888, surface 1 is RGB555.
5. GDI draws to surface 0 with a 1600-byte pitch. CRTC `0x28` is 2, DAC
   general control is `0x00100100`, and scanout uses 15 significant color bits
   in a 16-bit pixel. The framebuffer shows wrong colors and doubled pixels.

After the fix:

1. The channel-switch interrupt retains the format method in the FIFO.
2. The RM restores its saved state while PGRAPH FIFO access is disabled.
3. The retained method executes after interrupt acknowledgement and FIFO
   re-enablement, setting surface 0 to RGB555.
4. Final surface configuration is `0x66`; pitch remains 1600 and DAC/CRTC
   configuration is unchanged. The captured Windows framebuffer has the
   expected blue background, gray controls, and normal pixel spacing.

## Implementation and validation

`riva128_pgraph_command_submit` now returns whether it accepted a method.
It reports the requested channel through `CTX_USER` before raising the
context-switch interrupt, and defers execution while that interrupt is pending
or PGRAPH FIFO access is disabled. Both CACHE0 and CACHE1 advance GET only
when submission succeeds, including object-bind methods.

Run the standalone FIFO regression test:

```sh
python3 tests/video/vid_riva128_fifo_test.py
```

The FIFO test compiles the actual submission and puller functions with mocked
RM/register dependencies. It checks disabled FIFO access, interrupt deferral,
restore ordering, exactly-once execution, object binding, and CACHE1 wraparound.
The pre-fix implementation fails the queue-preservation assertion. The earlier
RGB-to-RGB555 packing fix alone does not address the channel-switch failure.
