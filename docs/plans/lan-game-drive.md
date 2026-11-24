# Play a paired openSU's Windows game from where it is installed

Requested 2026-11-24: "tablet and macOS can play the games installed here using wine but not via
streaming but more like a network drive without having to configure a network drive", "for easier
testing": the Mac and the tablet run fedora's installed Windows games under their own Wine, so a
runner change there is checked against real games without copying each one over first. Players get
the same row.

## What it does

A paired openSU's Windows game (Steam through fumarole, GOG, Epic, Own), on its details page, has
**Play from <host>** wherever this device runs Windows games under Wine: the game runs here, on
this device's CPU and GPU, with its files read from the host as Wine asks for them. Nothing is
copied first, and nothing is set up: no share, no mount, no password beyond the pairing.

- Reads come from the host's existing install file service (`/lan/roms/file`, `/lan/roms/chunk` of
  kind `install`, `docs/design/lan.md`), sealed like every LAN request, and are kept in a block
  cache here (`<cache>/lan-drive/<host>/<install>`), so a second run reads from disk.
- The game sees a read-only folder. A write inside it (a config or save some games keep beside the
  executable) goes to a layer here (`<data>/lan-drive/<host>/<install>`), which later reads see first.
  Saves in the Wine prefix are this device's, as for a local game.
- The launch goes through the same runner as a local copy of that game (`wine::prepare`, fumarole
  for Steam), pointed at the mounted folder.
- The host must be reachable while the game runs. A read that fails ends the game with the reason;
  it does not hang Wine.

## One file source, one mount per OS

`lan::InstallDrive` (guest) owns the file tree: the host's file list (`POST /lan/roms/install`),
ranged sealed reads, the block cache, read-ahead along a file being read in order, and the local
write layer. Each OS exposes it as a folder in the way that OS allows without root or setup:

| OS | Exposed as | Needs |
| --- | --- | --- |
| Linux | FUSE (`libfuse3`, `fusermount3`, unprivileged) | the fuse3 package; refused naming it when missing |
| macOS | a WebDAV server on loopback inside openSU, mounted with the system's `mount_webdav` on a folder openSU owns | to verify on the Mac: a user mount without root or a prompt |
| Android | no mounts exist for an app: the FEX syscall layer that runs Wine maps the game folder's path to `InstallDrive` | `windows-games-on-android.md`'s Wine and FEX first |

Only one implementation of the reads, cache and write layer exists (`InstallDrive`); each OS piece only
translates its file system calls to it.

## Order

1. `InstallDrive` with its cache and write layer, tested against a fake host's file service.
2. Linux FUSE, proven with two hidden openSUs: a GOG or Own Windows game installed only on the
   host, played from the guest under Wine, its first scene shown on the guest's `/frame.png`.
3. macOS `mount_webdav` on the Mac (opensu-mac), the same proof against fedora's host.
4. Android after `windows-games-on-android.md` runs a local Windows game on the tablet.
