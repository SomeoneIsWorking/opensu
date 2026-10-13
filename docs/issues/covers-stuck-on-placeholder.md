# Home covers can stay on the loading placeholder

Status: fixed (the fix still needs `ShellApp::publishSnapshot` to set `artworkPending`).

Observed: in `tools.screenshots --parity` runs of the same build, some Home captures showed covers
(Teeworlds, SuperTux, others) on the loading spinner and failed `home` against a good set (mean
5.8 to 9.4). The covers were never stuck. The log shows their textures uploaded on the frames just
after the capture.

Cause: `tools/screenshots/cli.py:_capture` took each frame after a fixed 1.2 s settle, while
`ui/image_decoder` was still decoding the shelf's covers on its one thread (two images per Steam
game, slower when the machine is busy or the catalog arrives twice and `TileArtwork::beginShelf`
starts the decodes over). `/state` had nothing to wait on.

Fix: `TileArtwork::pending` counts the window's tiles (and the header) for which
`Tile::awaitingArt` holds: no texture yet, and art downloading, queued, decoding, or decoded but
not yet uploaded. This is the same rule the tile painter's spinner uses. `/state` reports it as
`artworkPending`, and `_capture` waits for 0 before every frame. `tile_artwork_test` covers a
decoded cover that is still pending until it is uploaded.

Evidence: with the old capture, 2 of 2 runs failed `home`, and `ref-c` failed earlier (1 of 3).
With the wait, `home` passed in 6 of 6 runs (mean 0.17 to 0.35).
