## Project Slippi
This is a fork of Nintendont is used to support [Project Slippi](https://github.com/JLaferri/project-slippi)
on Wii hardware, and is specifically built for booting into _Super Smash Bros. Melee_
and applying specialized patches. Because this is the case, please be aware that there
are *no guarantees* that this fork will act predictably in any other circumstances.

_Project Slippi Nintendont only officially supports the NTSC 1.02 version of Melee._

### Features
- Automatically injects Project Slippi code for recording replays
- Support for writing Slippi replays to a storage device (USB drive/SD card)
- Support for streaming replay data over Wi-Fi or a USB Ethernet adapter
- Allows users to toggle various codes before booting into Melee:
	- Controller fixes (permanent UCF, stealth UCF, in-game toggleable UCF/Ardiuno fixes)
	- Stealth Tournament Mods (Neutral Spawns, hidden nametag when invisible, keep nametag during rotation)
	- Tournament Mods (Stealth Mods, D-Pad for rumble, stage striking)
	- Quality-of-life Mods (Tournament Mods, skip results, salty runback)
	- NTSC-to-PAL conversion
	- Lag Reduction (PD or PD/VB)
	- Ruleset Mods (Vanilla or Frozen Stadium)
	- Widescreen (None or Wide)

## Requirements/Instructions
Slippi Nintendont supports booting Melee from USB/SD card.
Booting Melee from the disc drive _should_ also work, but this is currently
un-tested and not fully supported.

_All storage devices must be formatted with a FAT32 filesystem._ In order to boot
from the Homebrew Channel, some storage device must be prepared with the
following files:

```
/ SD Card Root
├── apps/
│   └── Slippi Nintendont/
│       ├── boot.dol		# Nintendont Slippi DOL
│       └── meta.xml		# Homebrew Channel title data
└── games/
    └── GALE01/
        └── game.iso		# Vanilla NTSC v1.02 ISO
```

## Implementation Overview and Development Notes
This is a running list of noteworthy changes from upstream Nintendont:

- Emulate a Slippi EXI device on slot A (channel 0, device 0) for consuming DMA
  writes from the "Slippi Recording" Gecko codeset
- Force disable of TRI arcade feature (using freed space to store Melee gamedata)
- Implement [an approximation of the] standard POSIX socket API for networking
- Inject a Slippi file thread for writing gamedata to a USB storage device
- Inject a Slippi server thread for communicating with remote clients over TCP/IP

### Quick Nintendont Overview
Nintendont has two main components:

- A "loader" running on the PowerPC processor, used for configuring options and
  booting into a Gamecube game (see `loader/`)
- A "kernel" injected into the ARM co-processor, which acts as a middleman
  between some Gamecube game and IOS/hardware components

### Wii Networking
Network initialization should succeed assuming that the user has a valid network
connection profile, and that IOS is able to actually establish connectivity.

Note that Melee will crash/freeze on boot if network initialization is not delayed
by some amount of time (currently it seems like only a few seconds of delay is
sufficient). This is probably due to an increased amount of requests to the
Nintendont kernel during boot time (i.e. lots of disc reads during boot, for which
Melee expects data to be returned in a timely manner).

The interface for dealing with sockets is implemented in `kernel/net.c`.
This is a port of [the libogc implementation](https://github.com/devkitPro/libogc/blob/master/libogc/network_wii.c) with only minor differences (in order to account for differences
when running solely in ARM-world).

### Dealing with IOS `ioctl()`
When issuing ioctls to IOS, pointer arguments should be be 32-byte aligned.
You can either (a) get an pointer on the heap with the `heap_alloc_aligned()`
syscall, (b) put something on the `.data` section with the `aligned(32)` attribute,
or (c) use the `STACK_ALIGN` macro to align structures on the stack.

### ARM/PPC Cache Weirdness
Exercise caution when trying to coordinate reads/writes between PowerPC and the
ARM coprocessor. They keep separate caches, and there may be some situations
where you need to use cache control instructions to make sure that they don't
have conflicting views of main memory.

### Debugging and Log Output
Nintendont mostly uses a function `dbgprintf()` for logging. This function does two things:
	
	- Writes to the EXI bus (specifically, a USB Gecko); this depends on
	  some IOS patches (applied at boot) that modify an ARM service call
	- Writes to a log on the SD card (if logging is enabled in Nintendont options)

Typically, after booting into a game, `dbgprintf()` calls are very unreliable
if you're writing logs to an SD card. Additionally, we also believe there are
cases where attempts to write on SD may make things unstable and cause crashes
in PPC-land (see the section on **USB/SD Disc Accesses**).

With certain Melee patches, it's also possible to use something like the `ppc_msg()`
macro (see `kernel/SlippiDebug.h`) for writing messages to the screen in-game.
Approaches like this might be useful if you don't have a USB Gecko and would prefer
to rely on visual feedback in-game.

The thread for sending broadcast messages (`SlippiNetworkBroadcast`) has also been
used successfully for sending debugging data to a client over the network.
Sometime in the future, it's likely that some optional, generic debugging/profiling
features will exist in-tree (for now, this is up to you to implement if you need it).

### USB/SD Disc Accesses
At the front-end of the stack, Nintendont uses [FatFS](http://elm-chan.org/fsw/ff/doc/appnote.html)
to expose an interface for doing file operations on USB/SD. The underlying Nintendont
code for USB and SD card works by passing messages to the actual IOS USB/SD drivers.

