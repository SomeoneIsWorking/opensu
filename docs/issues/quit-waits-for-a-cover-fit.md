# A quit waits for the SteamGridDB cover being fitted

Status: open.

Observed 2026-11-04: in a hidden run on the player's library while SteamGridDB covers are being
fitted, `POST /quit` and SIGTERM take 1 to 3.5 s to exit, against 0.03 to 0.06 s with
`OPENSU_STEAMGRIDDB_API` pointed at a closed port. gdb during the wait: the main thread in
`ShellApp::~ShellApp` → `SteamGridDbCovers::~SteamGridDbCovers` →
`artwork::CoverFitter::~CoverFitter` joining its worker, which checks its stop token only between
games, so it finishes the fit (SteamGridDB requests and the image) under way first. Not traced
further.
