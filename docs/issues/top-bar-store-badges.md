# Top bar shows Epic and GOG unadded, and no Steam

Status: fixed; shown on the Ally 2026-11-27: the top bar shows only Steam, ready (`launchers: steam=ready`).

Reported by the player (2026-11-27, Ally, 3c200b21): "top right shows epic and gog even though not
integrated, steam is missing even though integrated".

Cause: `launcher_status.cpp:launcherBadges` took Steam's badge from the Steam client only, which
does not run when Steam comes through fumarole, and showed a store that needs a sign-in as failed
rather than not added.
