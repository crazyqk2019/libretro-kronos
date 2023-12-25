# Kronos libretro

## How to build
mkdir build_retro; cd build_retro;
make -j4 -C ../yabause/src/libretro/ generate-files
make -j4 -C ../yabause/src/libretro/
then execute retroarch -L ../yabause/src/libretro/kronos_libretro.so path_to_your_iso

Library will be built in the libretro subfolder.

You'll need retroarch (or another libretro frontend) to run this library.

## How to run games
Command line looks like this :

`retroarch -L path/to/library path/to/iso/or/rom`

## Where do you copy the bioses
Bioses go to the SYSTEM_DIRECTORY/kronos directory, saturn bios is expected to be named `saturn_bios.bin`, for ST-V bios you just need to copy `stvbios.zip` from MAME

## Currently known issues
- The core crashes when you switch from fullscreen to windowed on the fly.
- You won't be able to run this on macosx at the moment (retroarch lacks glsl3.3 support for macosx)
- It was mostly tested under linux, there could be issues on windows

-on standalone version Compute shader is recommended video core
