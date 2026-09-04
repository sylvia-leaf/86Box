PCBox
=====

PCBox is a low-level PC emulator based on 86Box.
It is licensed under GPLv2 or later.

You can find CI builds under our Actions tab.

Visit our Discord! https://discord.gg/QeaGZSUUcf

Or, join us on XMPP/Jabber at pcbox@chat.yax.im

Our website is at http://pcbox-emu.xyz

---

## Fork branch information

### What this branch does

This branch of PCBox completes the AMD Athlon emulation. Instead of attempting an AMD 751 (Irongate) northbridge emulation, or an emulation of any other AMD chipset for that matter, we derived a VIA KX133 emulation using existing VIA Apollo Pro133A and KT133 documentation and existing VIA Apollo emulation (the original KX133 datasheet is lost).

One machine containing a VIA KX133 chipset is also included, and an emulation of the IC Works W210 clock generator designed for VIA Athlon chipsets such as the KX133 and KT133. Downclocked CPUs and an AMD Generic option (similar to existing Intel Generic) are also included.

Enough of the Athlon (including the SMM, which is famous for its documentation still being unavailable publicly), has been emulated to boot a BIOS and run the Athlon path on 3DMark 2000, and enough of the chipset is emulated to install the VIA 4-in-1 drivers, version 4.35V.

### Current limitations

Performance on an Argon-500 with a Voodoo3 is currently at around 60% running 3DMark 2000 on a Ryzen 5 9600X host system. Current host systems are not fast enough to run high-speed Athlons. This is exacerbated by the fact that most downclocked Athlon CPUs are unselectable due to AMD's decision to represent the multiplier as an MSR. The minimum multiplier represented by FIDs is indeed 5.0x, and for downclocked Athlons, we keep the FID representation at 5.0x.

Without such a mitigation, the BIOS clock display becomes incorrect because it reads a multiplier of 1 and due to an unsigned-byte representation of the bus speed, wraps around modulo 256. For example, a 300 MHz CPU would appear as 44 MHz (44x1.0). While forcing FID to be 5.0x causes calculated bus speeds to be incorrect, we see this as causing less problems; it is a tradeoff because Athlon CPUs with clock speeds below 500 MHz were never released in real life. For this reason, the DRAM clock can also be incorrectly reported and result in the BIOS complaining that it should be 133 MT/s.

The KX133 implementation is based on publicly-available PCR files, particularly the ones from H.Orca and maybe other sources, which means that behaviour cannot be guaranteed to be identical to a real hardware KX133. Such behaviour must be tested in more detail at a later date. If the KX133 datasheet resurfaces, more work will be done to ensure that the emulation is accurate.

Because the current implementation includes KX133, this also means that only Slot A processors are currently available. While the instruction set infrastructure (mostly?) exists to emulate the Athlon XP in PCBox, Socket A CPUs are not currently emulated.

The incomplete AMD 751 emulation remains in the tree but is currently inaccessible. Work will definitely be done to get it functional in the future, however it is likely that we will work on getting an emulation of the KT133 up and running before we restart work on finishing the AMD 751.

SMM mode emulation is also incomplete and/or inaccurate. Only enough of the SMM has been emulated to allow the currently-emulated motherboard to boot. ACPI shutdown is also currently non-functional. The $PIR table for the current machine needs to be verified. The emulated CPU is also incorrectly identified as an "AMD K6" on CPU-Z vintage edition, version 1.04, and we're not sure why this is the case.

### Current scope

Right now, work will be done to get more KX133 machines up and running to validate the KX133 emulation. Once we are comfortable with KX133 emulation, we will move onto KT133 emulation (which should hopefully be quite similar to KX133) including Athlon XP and Duron CPUs. 

AMD 751 support is currently postponed. Support for AMD 751 motherboards is planned to include only those that used a hybrid chipset (i.e. 751 northbridge + VIA southbridge). Emulation of the AMD 756 (Viper) southbridge and motherboards with a full-AMD chipset (751 + 756) comes afterwards. Similarly, we plan on waiting for full DDR support on PCBox's main branch (which is a requirement for Intel 845E emulation, which is planned) before starting work on KT266-era and AMD 760-series chipsets.

Some dynarec support is planned to increase speed slightly, but don't expect it to make a 500 MHz Athlon run at full speed on most host systems.

### Phased work

This work is split into phases.

1. 
 a. Athlon bringup and KX133 boards
 b. Experimental dynarec work to increase speed 
2.
 a. Socket A and Athlon XP definitions
 b. KT133 boards
3.
 a. AMD 751 + VIA boards (finish that K7M!)
 b. AMD 751 + 756 boards
4.
 a. KT266 boards and later
 b. AMD 761, 762, 766, 768 support

### AI assistance

Development was assisted by AI. Check commit details for more information.

### Contribution

If PCBox wishes to use this fork, then they can contact me. But I hope to make it a bit more stable first.