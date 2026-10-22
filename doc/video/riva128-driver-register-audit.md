# RIVA 128 register audit against `riva128drivers/nv3rm.vxd`

The in-tree Windows 9x resource manager is the source of truth for which NV3
MMIO registers exist and how the emulator must decode them. This note records
how to read it and the discrepancies that were found and fixed.

## Reading `nv3.vxd` / `nv3rm.vxd`

These are 32-bit LE (linear executable) files. `objdump` cannot follow the LE
container, so extract object 1 to a flat image first:

- The LE header is at `e_lfanew` (DWORD at file offset `0x3c`).
- Object table offset is at LE+`0x40`, object count at LE+`0x44`, object page
  table offset at LE+`0x48`, and the page-data offset at LE+`0x80`.
- For `nv3rm.vxd` the code object is 695 x 512-byte pages and the page data
  begins at `0xac00`, so the flat image is `dd if=nv3rm.vxd bs=1 skip=0xac00
  count=0x56d1c`. (The per-page table entries are page numbers, not file
  offsets; do not use them as offsets.)

Disassemble the flat image with `radare2` (`r2 -b 32 -a x86 -m 0x0`). Addresses
in the RM match the offsets quoted in the other `riva128-*.md` notes.

## How the RM finds MMIO registers

Every MMIO access loads the base pointer first:

```
mov eax, [<device>]
mov eax, [eax * 4]
mov eax, [eax + 0x456e]     ; MMIO base
mov eax, [eax + <offset>]   ; register read
mov [eax + <offset>], ...   ; register write
```

Filtering the disassembly on that base load produces a reliable list of the
registers the driver uses. Auditing the emulator against that list found:

### Fixed

- **CACHE0 PUSH0/PUSH1 (`0x003000`, `0x003004`) were write-only.** The RM saves
  (`0x188b9`, `0x1896d`) and later restores both around its software command
  submission, so reading them back as zero silently disabled CACHE0.
  `riva128_pfifo_read()` now returns `caches[0].push_enabled` and
  `caches[0].chanid`.

- **CACHE1 RAM window indexing (`0x003300`+).** The RM reads a command slot as
  `mmio[0x3300 + (GET >> 2) * 8]` (`0x1d398`..`0x1d45c`), i.e. indexed by the
  raw pointer value. The emulator stored MMIO writes through an extra
  `normal2gray()` and therefore wrote a different slot than it read for every
  odd index. `riva128_pfifo_write()` now indexes `cache1[]` by the raw slot,
  matching `riva128_pfifo_read()` and the RM.

- **PGRAPH ROP (`0x400624`) was write-only.** It is part of the context the RM
  saves/restores; `riva128_pgraph_read()` now returns `pgraph.rop`.

- **VGA aliases in the MMIO window.** `riva128_mmio_read_l()` decoded the
  `0x6813c6`..`0x6813cb` aliases into a temporary and then fell through to the
  range handlers. Masking the address to a DWORD moves it into the PRAMDAC
  range, so the VGA byte was replaced by a bogus PRAMDAC read. The aliases now
  return directly, matching `riva128_mmio_read()` and `riva128_mmio_read_w()`.

- **Debug leftovers in `riva128_pgraph_execute_command()`.** A malformed
  `pclog(...); if (...) pclog(...)` blob logged unconditionally on the hot
  method path; it was removed. The invalid-patch-config `warning()` also
  printed the ROP instead of the patch configuration.

## Remaining gaps (not fixed)

The same audit shows registers the RM reads/writes but the emulator ignores.
None are known to break the validated modes, but they are candidates:

- PRAMDAC `0x680204`, `0x68021c`..`0x680244`, `0x680280`..`0x68028c`. The RM's
  mode set (`0x4bda2`..`0x4c64d`) writes and reads these back. The emulator
  models the PLLs only at `0x680500`/`0x680504`/`0x680508`.
- PGRAPH `0x400500`..`0x40053c`, `0x400540`..`0x400570`, `0x400600`..`0x40061c`
  and `0x400680`/`0x400690`..`0x4006a0`. These are saved/restored D3D-context
  registers; the emulator returns zero for the ones it does not model.

## Regression tests

`tests/video/vid_riva128_test.c` now covers the CACHE1 RAM slot addressing and
the CACHE0/PGRAPH read-backs. Build and run it standalone (the tree's
`build/` may not have the target configured):

```sh
gcc -O0 -g -ffunction-sections -fdata-sections -fno-strict-aliasing -UNDEBUG \
    -o /tmp/riva128_tests tests/video/vid_riva128_test.c \
    -Isrc/include -Ibuild/src/include -Wl,--gc-sections -lm
/tmp/riva128_tests
```
