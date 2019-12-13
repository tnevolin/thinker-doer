
SMACX Thinker Mod
=================

Thinker is an AI improvement mod for Alpha Centauri: Alien Crossfire. By patching the game to use an additional dll, new functionality can be developed in C++. This mod increases the level of challenge in single player while providing many gameplay enhancements. Some of the features include:

* Vastly improved production/terraforming AI
* Rewritten code for AI social engineering choices
* AI terraformers prioritize condenser-farms, boreholes and forests
* AI builds more crawlers and deploys them better
* AI deploys colony pods much sooner instead of wandering on the map
* Unit auto design feature creates improved probe teams and AAA garrisons
* Many enhancements to random map generation/spawn locations
* Does not affect player-run factions (with some exceptions)
* Minimal changes to vanilla game mechanics
* Game binary includes [Scient's patches](Details.md)

This mod is tested to work with the [GOG version](https://www.gog.com/game/sid_meiers_alpha_centauri) of Alpha Centauri.
More information about the features and recommended settings can be found from [Details](Details.md).


Download
--------
Currently the latest release is v1.0. See also the [Changelog](Changelog.md).

* [Release versions](https://www.dropbox.com/sh/qsps5bhz8v020o9/AAAp6ioWxdo7vnG6Ity5W3o1a?dl=0&lst=)
* [Develop builds](https://www.dropbox.com/sh/qsps5bhz8v020o9/AADv-0D0-bPq22pgoAIcDRC3a/develop?dl=0&lst=)


Installation
------------
1. Extract the files to Alpha Centauri game folder.
2. Check the changelog for release notes.
3. Change configuration from thinker.ini or just use the defaults.
4. Start the game from terranx_mod.exe
5. Mod version/build date should now be visible in the game version menu (Ctrl+F4). If it is not displayed, Thinker is not correctly loaded.


Other mods
----------
* [SMAC-in-SMACX mod](Details.md#smac-in-smacx-mod) can be installed to play a game similar to original SMAC from the SMACX game binary while Thinker is enabled.
* [Pracx](https://github.com/DrazharLn/pracx) graphics enhancement patch can be used together with Thinker, but it is not required in any way. For easiest installation, download [version 1.11 or later](https://github.com/DrazharLn/pracx/releases/). If you already use older Pracx 1.10 you can just rename terranx_mod.exe to terranx.exe and run pracxpatch.exe so that the installer patches the right binary.


Compiling
---------
Thinker can be compiled with GCC and the included CodeBlocks project. GCC version 8.1.0 or later is recommended and can be found from [mingw-w64 project](https://sourceforge.net/projects/mingw-w64/files/).


License
-------
This software is licensed under the GNU General Public License version 2, or (at your option) any later version of the GPL. See [License.txt](License.txt) for details.

The original game assets are not covered by this license and remain property of Firaxis Games Inc and Electronic Arts Inc.

Sid Meier's Alpha Centauri and Sid Meier's Alien Crossfire is Copyright © 1997, 1998 by Firaxis Games Inc and Electronic Arts Inc.
