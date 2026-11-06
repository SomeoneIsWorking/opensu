# A ROM folder with loose files shows nothing

Observed on the tablet with 0.6.0: picking a folder that holds `link_clock.gba` directly adds nothing
to Home and says nothing past the "ROM folder added" toast. Moved into `gba/` inside that folder, the
game shows. The scan reads only subfolders named for a console (`rom_systems.cpp:systemForFolder`),
on every platform, and the tile's "One folder per console" is the only hint.

Likely fix: place loose files by an extension only one console uses, and say so when a picked folder
yields no games.
