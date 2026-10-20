# Switch games show no art

Status: fixed.

Observed: every tile under Library › Nintendo Switch showed iiSU's first-letter placeholder and
the file or folder name ("XCX_DE", "TOTK"), with `/mnt/Boy/ROM/Switch` holding 27 games: loose NSP
and XCI files, and folders of a base file, its update, DLC and `.url` junk.

Cause: `library/roms.cpp:Provider::list` took a game's own title and icon only from
`readDumpInfo`, which reads PS4 dumps; libretro-thumbnails has no Switch set, so
`artwork::ArtworkStore::pathFor` had no source either. A Switch game's title and icon are inside
its file, in the Control NCA, encrypted with keys only the player has.

Fix: `library::roms::SwitchInfo` (with `switch_package`, `switch_nca`, `switch_keys`,
`switch_crypto`) reads them offline with the player's Eden keys and keeps them in
`<cache>/artwork/switch/` by file size and mtime; `Provider::list` uses it for the `switch`
system as it uses `readDumpInfo` for PS4. In a folder the base file is the one whose `[title id]`
ends in `000` (`rom_systems.cpp:isAddOn`), else the largest file that is not an update.

Proof: `scratch/switch-art/` hidden run (`start.sh`, `drive.py`), Eden's real keys read in place:
`shell-first-run.log` `27 of 27 Switch games titled from their own files (27 read now, 0 from the
cache, 0 failed)`, `shell-cached-run.log` `(0 read now, 27 from the cache, 0 failed)`; `frame.png`
and `frame-switch-end.png` show all 27 icons, `frame-switch-xcx.png` the NACP title "Xenoblade
Chronicles X: Definitive Edition" for `XCX_DE.nsp`. `switch_info_test` builds NSP and XCI files
encrypted as Nintendo's (key area and title key NCAs, tickets, derived keys, a wrong header key).
`scratch/switch-art/keycheck` showed every derived key equal to the real prod.keys' own (names
only, no values printed).

Not covered: `.nsz`/`.xcz` whose Control NCA is compressed (`.ncz`), NCA2 and older headers, key
folders other than Eden's.
