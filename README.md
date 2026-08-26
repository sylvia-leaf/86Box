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

While the Intel 850 machine provided is at least partially functional to the point that it can boot an operating system and initialise Intel chipset drivers, it requires more testing.

Only one timing range has been implemented in the RDRAM SPD, and currently all emulated RIMMs will declare PC800. 

### Current scope

While only the Intel 850 chipset is currently available, scope will extend to the Intel 820 (ICH1), Intel 820E (ICH2), and Intel 850E (ICH2) chipsets. When official support for the 82845E MCH becomes available on PCBox, then we will implement some Intel 850E (ICH4) machines.

More interesting Intel 850 machines will also be coming in the future; because there were very few Intel 820E boards (I could only find evidence of the ASUS CUC2, which doesn't seem that interesting to me), most of the other work will be focused on ICH1 emulation.

For now, the Intel 840 (Carmel) and Intel 860 (Colusa) chipsets are out of scope, however if support for SMP becomes stable on PCBox, then they may be added in the future. The SiS R658 remains out of scope because the datasheet appears to be lost and I don't feel like reverse engineering something only the Abit SI7 really used (most of my work is currently on supporting the Mach64-based ATI Rage series). The 82805AA MTH, which was meant to allow manufacturers to make Intel 820 boards using SDRAM, but was famously broken, is also out of scope for now, as are any Intel Memory Repeater Hubs. 32-bit RIMMs are currently out of scope.

However, scope can change, and apart from the SiS R658 (and the unreleased SiS R659 quad-channel RDRAM chipset), nothing is off the table.

### AI assistance

Development was assisted by Claude Opus 5, Gemini 3.7 Flash, GPT-5.6 Sol and possibly other AI models.