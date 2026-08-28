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

This branch of PCBox adds experimental support for Direct RDRAM by adding RDRAM/RIMM detection support logic, including RDRAM devices, RIMM channel distribution, SPD data and enough communication to boot a machine that uses the Intel 850 (Tehama) chipset. 

Additionally, a machine configuration with the Intel 850 (Tehama) chipset is provided, with 82850 MCH (northbridge) emulation based on the existing PCBox 82845 MCH emulation with changes as documented by Intel datasheets.

### Current limitations

RDRAM timings are currently not implemented, and are honestly unlikely to be emulated in the foreseeable future considering how difficult it already is to emulate a Pentium 4 at full speed.

Currently, only the Intel 850 chipset is available due to lack of ICH1 emulation, and my current lack of interest in the Intel 820E chipset. However, I will likely work on some experimental ICH1 support so Intel 820 boards will become available.

While the Intel 850 machines provided are at least partially functional to the point that they can boot an operating system and initialise Intel chipset drivers, they require more testing. For example, the Gigabyte machine currently does not handle a soft reset properly.

Only one timing range has been implemented in the RDRAM SPD, and currently all emulated RIMMs will declare PC800. 

### Current scope

While only the Intel 850 chipset is currently available, scope will extend to the Intel 820 (ICH1), Intel 820E (ICH2), and Intel 850E (ICH2) chipsets. When official support for the Intel 845E chipset, including the 82845E MCH and ICH4, becomes available on PCBox, then we will implement some Intel 850E (ICH4) machines.

More interesting Intel 850 machines will also be coming in the future; because there were very few Intel 820E boards (I could only find evidence of the ASUS CUC2, which doesn't seem that interesting to me), most of the other work will be focused on ICH1 emulation. Once that is done, we expect to work on not only the 82820 MCH, but also the 82805AA MTH (Memory Translator Hub, the infamously broken RDRAM-to-SDRAM translator), and support for boards that use 32-bit RIMMs.

For now, the Intel 840 (Carmel) and Intel 860 (Colusa) chipsets are out of scope, however if support for SMP becomes stable on PCBox, then they may be added in the future. The SiS R658 remains out of scope because the datasheet appears to be lost and I don't feel like reverse engineering something only the Abit SI7 really used (most of my work is currently on supporting the Mach64-based ATI Rage series).

However, scope can change, and apart from the SiS R658 (and the unreleased SiS R659 quad-channel RDRAM chipset), nothing is off the table.

### Phased work

This work is split into phases, and honestly, it's not just RDRAM, so maybe there is a better name for the branch than "rdram".

1.
 a. Basic RDRAM support and Intel 850
 b. Intel 850E
2.
 a. Basic ICH1 support
 b. Intel 815/815P
 c. Restoring all 815E/815EP boards that were in PCBox at one point
3. Intel 820 MCH support
4. Intel 820 MTH support
5.
 a. 32-bit RIMM boards
 b. 440 boards that use the Gluechip 2

### AI assistance

Development was assisted by Claude Opus 5, Gemini 3.7 Flash, GPT-5.6 Sol and possibly other AI models.

### Contribution

If PCBox wishes to use this fork, then they can contact me. But I hope to make it a bit more stable first.  