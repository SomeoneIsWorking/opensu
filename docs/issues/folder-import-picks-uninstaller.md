# Folder import picks a Windows game's uninstaller

Status: fixed; shown on the Ally 2026-11-27: the entry starts `SpongeBob_BFBB_R.exe`.

Reported by the player (2026-11-27, Ally, e040b6d1): "battle for bikini bottom windows game picked
the uninstaller when using folder import".

Cause: `game_folder.cpp:notAGame` knew Inno Setup's first uninstaller, `unins000`, by name only.
The game's folder also holds `unins001.exe` (1.3 MB), which beat the 271 KB `SpongeBob_BFBB_R.exe`
on size in `programIn`. Fixed: `unins` and any number is an uninstaller.
