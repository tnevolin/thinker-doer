
Thinker command line options
============================

    -smac       Activate SMAC-in-SMACX mod. Implies smac_only=1 config option.
    -native     Start game in fullscreen mode using the native desktop resolution.
    -screen     Start game in fullscreen mode using the resolution set in mod config.
    -windowed   Start game in borderless windowed mode using the resolution set in mod config.


Thinker keyboard shortcuts
==========================

These shortcuts are always available whether debug build is available or not.

    Alt+T       Show Thinker's version and options menu (use Alt+H when PRACX is enabled).
    Alt+R       Toggle tile information update under cursor if smooth_scrolling is enabled.
    Alt+O       Enter a new value for random map generator when scenario editor mode is active.
    Alt+Enter   Switch between fullscreen and windowed mode unless desktop resolution is used in fullscreen.

These developer mode shortcuts are only available if Thinker is using debug build.
Some of these options require extended debug mode.

    Alt+D   Toggle extended debug mode.
    Alt+M   Toggle verbose logging mode on debug.txt.
    Alt+V   Iterate various TileSearch instances for the selected map tile.
    Alt+F   Iterate various move_upkeep overlays.
    Alt+Y   Toggle faction diplomacy treaty matrix display.
    Alt+X   Run pathfinding between two previous map tiles.
    Alt+Z   Print debug information for the selected map tile.
    Shift+4 Toggle world site priority display for the legacy AI.
    Shift+5 Toggle goals overlay display on the map.
    Shift+* Toggle faction border display on the map.

As a general troubleshooting feature, all Thinker builds also include a custom crash handler that
writes output to `debug.txt` in the game folder if the game happens to crash for any reason.


Tech tree visualization
=======================
As an additional feature, this repository includes a script written in Python 3 that can be used to visualize
any custom tech trees defined in alphax.txt. Techs are strictly aligned based on their level in the output
to make it easier to visualize the research order. To use the script, first install dependencies.

    pip install networkx matplotlib

Then plot any possible tech tree defined in alphax.txt to a png file.

    ./tools/techvisual.py -f techs.png /path/to/alphax.txt


Compiling Thinker
=================
Install dependencies to compile Thinker on Windows using CodeBlocks:

* [CodeBlocks](https://www.codeblocks.org/downloads/)
* [MinGW toolkit](https://github.com/niXman/mingw-builds-binaries/releases/download/12.2.0-rt_v10-rev2/i686-12.2.0-release-posix-dwarf-msvcrt-rt_v10-rev2.7z)

To avoid compiling issues, do not use the older MinGW toolkit version bundled with CodeBlocks.
GCC versions older than 8.1.0 are not supported. After installing MinGW, new paths need to be
entered in Settings > Compiler > Toolchain Executables for the default GNU GCC Compiler profile.

Another way to build Thinker on Linux platforms is to convert the CodeBlocks project file into
actual Makefiles and then compile them using g++-mingw-w64. A build script that does all these
steps is provided in `tools/compile` folder.

1. First install dependencies: `apt install build-essential g++-mingw-w64-i686`
2. Clone [cbp2make](https://github.com/dmpas/cbp2make) and compile it from sources.
3. Install `cbp2make` binary on a folder included in the environment PATH.
4. Build Thinker: `./tools/compile/mingw-compile.sh`
5. Get compiled binaries from: `build/bin/`


Details for patch startup method
================================
Thinker's launcher program thinker.exe requires the original terranx.exe (game version 2.0) provided
by GOG in the same folder, (3 084 288 bytes, SHA-1 checksum: 4b19c1fe3266b5ebc4305cd182ed6e864e3a1c4a).
Using other official game binaries of the same version should also work. If an incompatible,
modified binary is found, the mod will display an error message and close the game.

Thinker will not modify terranx.exe file on disk, so it can still be used to start an unpatched
version of the game. Instead the mod will apply all of the changes at startup by patching
the binary in memory. Also Scient's patch v2.0 changes are automatically loaded at startup.

Source file `src/patchdata.h` contains Scient's Patch v2.0 changes that are applied at game startup.
Using a separate script this file can be converted into IDA PatchByte commands that can be used to
transform the default terranx.exe into a patched version that will also load thinker.dll at startup.
When the unmodified GOG binary is loaded in IDA, use this script to create the IDA PatchByte commands:

    ./tools/idapatch.py < src/patchdata.h

At game startup, Windows will attempt to load thinker.dll from the same folder before the game starts.
Note that Windows will terminate the process if any of the dll dependencies from the import table
are not available. When thinker.dll is loading, it will attempt to patch the game binary image
residing in the same address space before the actual game can start.

This way it is not required to write any of the actual patches to the exe file, instead they can be
applied dynamically at runtime. Thinker also performs some sanity checks during patching, and it
will close the game process with an error message if any of the checks fail.


Patching OpenSMACX to work with Thinker
=======================================
Due to the simplicity of Thinker's loading method, [OpenSMACX](https://github.com/b-casey/OpenSMACX/)
or the default terranx.exe can be patched manually in a hex editor to also load Thinker at startup.

Note that this combination of patches is not fully tested and some issues may appear due
to the combination of these binaries. After patching, OpenSMACX binary will not start
unless Windows can also load thinker.dll from the same folder.

1. Open `terranx_opensmacx_v0.2.1.exe` (or latest release) in a hex editor.
2. Replace any string `SHELL32.dll` with `thinker.dll`
3. Replace any string `ShellExecuteA` with `ThinkerModule`
4. Move all exe and dll files to the same folder with Alpha Centauri.

Thinker should now start along with OpenSMACX. Thinker version should always be visible in the game
version menu (Ctrl+F4). If this is not the case, Thinker did not start correctly with the game.
Check if you are actually running the game binary with the imports patched as described above.

