# ROM import over the LAN

Requested 2026-11-04: "I also want LAN ROM import so I can import ROMs from PC and play on my
phone".

## What it does

On a paired guest, Settings › LAN › ROMs on <host> lists the host's ROMs by console with their
sizes. The player picks games or a whole console, and the BIOS files the host has for those
consoles that the runner here lacks. Import copies them into the guest's own ROM folder through
the ROM folders' owner (`library::roms::RomTree`); on Android that is the folder picked through the
system picker, asked for when there is none. Each game shows its progress and the total; a
transfer goes on after the connection drops, and each file is checked against the host's SHA-256.
Imported ROMs appear in the guest's library and play on the built-in emulator. Linux to Linux works
the same. Protocol and owners: `docs/design/lan.md#rom-import`.

## Order

1. Linux both ways. Built (2026-11-04): `lan::RomShelf` on the host (only listed files, `..` and
   unlisted paths 403, ranges read on demand, hashing on its own thread), `lan::RomImport` on the
   guest (sealed 2 MiB chunks into resumable parts, SHA-256 checked, placed through
   `RomTree::place`), `app::LanRoms` with the pages, BIOS offer and progress; ROM folders now apply
   at once. Proven with two hidden openSUs over real Avahi (`scratch/lanrom/proof/`): the guest
   imported a homebrew Game Boy ROM and a copy of a commercial one from the host's scratch ROM
   folder (SHA-256s equal to the sources), both in its Game Boy shelf, the homebrew one playing on
   the runner; a 96 MiB file went on from its part (byte 25165824) after the host was killed by PID and
   started again, arriving equal to the source; signed, sealed asks for `notes.txt` (unlisted),
   `../Attic/roms/x`, a `..` path to the host's `lan.json` and `/etc/passwd` all 403 with the host's
   refusal lines.
2. Android: discovery through `NsdManager` with the multicast lock, pairing and the LAN listener on,
   the pairing card and Settings › LAN by touch and pad, import into the picked (`content://`)
   folder through `DocumentTree.place`. Built (2026-11-04). Proven on a Huawei MatePad (BKY-W09,
   Android 12) with a hidden openSU on this PC (`scratch/lanrom/tablet/`): LAN turned on in the
   first-run chooser by touch, the PC found over NSD and the tablet found by Avahi, paired with
   the pad's A (the control channel's injected pad, the screen showing pad prompts), ROMs on Desk,
   Game Boy and its game, Back and Import by touch; with no ROM folder, Import opened the system
   picker, a new folder was made and allowed, and the homebrew ROM arrived in
   `openSU-LAN-test/Game Boy/` with the source's SHA-256, showed in the Game Boy shelf and ran on
   the built-in mGBA after its download (frames `t21-game.png`, `t22-game-later.png`).
3. Windows and macOS discovery (DNS-SD through `DnsServiceRegister`/`DnsServiceBrowse` and
   `dns_sd.h`); the rest of import is portable already.
4. An Android host offering ROMs from a picked folder (reading ranges through
   `ContentResolver.openFileDescriptor`).
