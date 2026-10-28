# Screenshot credits

The README screenshots are frames of openSU itself (`opensu --hidden`, frames from
`GET /frame.png`), captured by `uv run --frozen python -m tools.screenshots --profile real` on the
maintainer's own machine, showing the maintainer's own library as openSU finds it: installed Steam,
Epic and GOG games and the maintainer's own ROM dumps.

- **Game art** (covers, heroes, ROM box art) belongs to its owners. It is shown only to depict what
  the launcher does with a real library; it is not part of openSU and is not licensed by this
  repository. openSU fetches it at runtime from the stores, SteamGridDB and the libretro thumbnail
  index.

openSU's own parts of every frame:

- Cal Sans (`assets/CalSans-Regular.ttf`), SIL Open Font License 1.1 (`assets/CalSans-OFL.txt`).
- Steam, Epic Games and GOG.com logos from [Simple Icons](https://simpleicons.org), CC0 1.0
  (`assets/icons/README.md`). The marks belong to their owners.
- Console cards and tab glyphs: openSU's own drawings of the hardware (`assets/consoles/`, MIT).
  No console maker's logo or box art is in them.
- Everything else (tiles, store cards, dock icons, menus, prompts) is drawn by openSU (MIT,
  `LICENSE`).

`library-iisu.webp` and `console-shelf-iisu.webp` (`--console-art iisu`) show the README's
showcase of the Console art setting:

- **iiSU's console cards and tab glyphs** belong to [iiSU](https://github.com/iisu-network/iiSU).
  openSU downloads them from iiSU's public release only for a player who chooses them; they are
  not part of openSU, not in this repository and not licensed by it. Consoles iiSU has no art for
  (the PS4) show openSU's own in those frames.

`--profile demo` captures the same screens offline in a throwaway home with a demo library of
freely licensed games (`tools/screenshots/demo.py`, art pinned by SHA-256 with its Wikimedia
Commons source, author and licence) into `scratch/screenshots/demo/`; those frames are not in the
README.
