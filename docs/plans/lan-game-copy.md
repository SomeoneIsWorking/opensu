# Copy a paired openSU's game to this device

Requested 2026-11-20: "Can we add an option like "copy to this device"?" and, after the ROM version
landed, "it should cover as much as possible".

## What it does

A paired openSU's game, on its details page, has **Copy to this device** whenever this device can
play the copy: the files come over the LAN (`lan::RomImport`'s sealed, resumable, SHA-256 checked
chunks), are placed where this device's owner of that kind of game keeps it, and that owner adopts
them, so the game shows as installed here and launches like one installed here. The row shows the
copy's progress and goes once a copy is here.

| Host's game | Placed in | Adopted by | Offered when |
| --- | --- | --- | --- |
| ROM | the first writable ROM folder, its console's folder (`RomTree::place`) | the catalog's next read | always (built: `740fc24b`) |
| Steam (Steam's or fumarole's install) | fumarole's apps folder, the host's folder name | `fumarole install <appid> --path <folder>`: keeps every chunk already on disk, fetches the rest | fumarole is here and signed in, and the host's install is a Windows build |
| GOG | `Paths::gameRoot(folder, id)/<host's folder name>` | an `InstallRecords` record with the host's platform | gogdl is here; a Linux build only on Linux |
| Epic | legendary's install dir, the host's folder name | `legendary import <app> <folder>` | legendary is here and signed in; a Windows build, or Linux on Linux |
| Own, found in a games folder | the first games folder | the next scan of it | the host found it in its games folder; an `.exe` anywhere Wine runs, a Linux program only on Linux |

Not offered: Lutris games and Steam shortcuts (their files are wherever their launcher keeps them),
Flatpaks (installed from their remote, not copied), a file the player picked from a shared folder
(its folder is not the game's alone), a Mac `.app` (its framework links do not survive a file-by-file copy).

## Protocol

`/lan/roms` lists `installs` beside the consoles: catalog id, store, store id, platform and folder
name. `POST /lan/roms/install {"id"}` lists that install's files (path, size, whether it runs),
walked when asked. Files are fetched with `/lan/roms/file` and `/lan/roms/chunk` asks of kind
`install` with path `<install id>/<path inside>`; the host resolves it in the install's current
folder and refuses anything outside it, links included. Details: `docs/design/lan.md`.

## Order

1. Done: the install folder of each installed game on `library::Game` (`installFolder`), filled
   by each store's reader.
2. Done: `RomShelf` lists installs and serves their files.
3. Done: `LanRoms::copyHere` for every row above, placed by `app::CopyPlaces`, taken over through
   `app::Installs::adopt`; the row offered from `LanRoms::copyState`.
4. Prove each kind between this PC and the Mac: copy, launch.
