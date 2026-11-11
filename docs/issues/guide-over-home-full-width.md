# The Guide menu over Home fills the screen, its hints over its last row

Status: open, found 2026-11-11 on origin/main 2e92f164 (and on the Channels branch on top of it).

Seen in a hidden openSU, 1280 x 800, demo library (`tools.screenshots --parity --scenes
home,library,details,guide`): the Guide menu opened over Home is a full-width column of 100 px
rows, Home's grid is hidden, and the hint bar (B Close, A Select) is drawn over the Settings row.
Expected, as the openSU build installed on 2026-11-07 still draws it: a left column of compact rows
(Home, Library, Search, Devices, Downloads, Settings, Power), the page's quick rows beside it, and
Home showing to the right.

Not traced yet. The Guide's layout changed after 2026-11-07 (`df4f651e`, portrait layout;
`1eac798f`; `e744a4fd`); bisect the `guide` scene between the installed build's commit and
2e92f164 to find the first frame that shows it.

Frames: `scratch/channels-theme/guide-before/guide.png` (installed build) and
`scratch/channels-theme/guide-origin/guide.png` (2e92f164); `scratch/` is temporary, so take them
again with the command above.
