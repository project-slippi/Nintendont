Download the `.zip` from the [latest release](https://github.com/project-slippi/Nintendont/releases/latest)

## Nintendont Slippi
This is a fork of [FIX94/Nintendont](https://github.com/FIX94/Nintendont) specifically
used for supporting Project Slippi.

## Users
See [README.md](https://github.com/FIX94/Nintendont/blob/master/README.md) for standard Nintendont if you're new to Nintendont.

### Replays
See documentation in [SLIPPI.md](SLIPPI.md) for more information.

### Kadano's Wii softmodding guide
I recommend using [Kadano's guide](https://docs.google.com/document/d/1iaPI7Mb5fCzsLLLuEeQuR9-BeR8AOwvHyU-FM8GKmEs) if you're new to Wii homebrew. Many guides in the wild are out of date or weren't very good to begin with.

### Installation Summary
1. Download the `.zip` from the [latest release](https://github.com/project-slippi/Nintendont/releases/latest). Unzip to the root of your SD card such that `boot.dol`, `meta.xml`, and `icon.png` all end up under `/apps/Nintendont Melee`.
2. Copy your vanilla Melee (NTSC 1.02) ISO (as well as any special versions like [Training Mode](https://www.patreon.com/UnclePunch)) to `/games/`.
3. Combine with [Priiloader](http://wiibrew.org/wiki/Priiloader) and [Nintendont Forwarder for Priiloader](https://github.com/jmlee337/Nintendont-Forwarder-for-Priiloader) to autoboot from power-on to Nintendont.
4. Turn on autoboot in Nintendont settings to autoboot all the way to Melee.

## Developers
See [README.md](https://github.com/FIX94/Nintendont/blob/master/README.md) for standard Nintendont if you're new to Nintendont.

### Build Requirements
You will need to install part of the devkitPro toolchain to build Nintendont. Visit [the getting started page](https://devkitpro.org/wiki/Getting_Started) and find the instructions for your system. I recommend installing GBA, Wii, and GC packages. GBA is required to get `devkitARM` which isn't provided in the others. You effectively need `devkitARM` `devkitPPC` and `libogc`.

### Building on Windows
To build on windows, you need to run the `Incremental Build.bat` file in the Nintendont root.

### Troubleshooting
libwinpthread-1.dll error:
```
make -C multidol
make[1]: Entering directory '/home/Jas/Documents/GitHub/Nintendont/multidol'
 ASSEMBLE    crt0.S
make[1]: *** [Makefile:36: crt0.o] Error 1
make[1]: Leaving directory '/home/Jas/Documents/GitHub/Nintendont/multidol'
make: *** [Makefile:34: multidol] Error 2
```
For some reason my devkitPro installer did not install things quite correctly for me. In order to fix the above error, you will need to go to your `devkitPPC/bin` directory, grab the file called `libwinpthread-1.dll`. You will need to copy this file to `devkitPPC/libexec/gcc/powerpc-eabi/8.2.0`. Repeat the same thing for the `devkitARM` directory, this should fix the issue.
