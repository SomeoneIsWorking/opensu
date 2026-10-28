# openSU's own default assets, iiSU's as an option, and new README screenshots

Status: open, planned; follows the UI toolkit's theme (`docs/design/ui-toolkit.md`).

iiSU's repository has no licence, so its sounds and nav icons are all rights reserved. openSU never
ships them but downloads them at run time from iiSU's public release (`artwork/apk_archive.hpp`,
Alpha 7.4), which is legally shaky for a public product and breaks for everyone if that release
changes.

Done: console cards and tab glyphs are openSU's own drawings (`assets/consoles/`, MIT) and the only
source; iiSU's starter pack and border-pack glyphs are no longer downloaded, with no iiSU option
for them. README shots are from the maintainer's own library (`tools.screenshots --profile real`).

Decided:
- Default: openSU's own set from freely licensed sources (Kenney's CC0 UI sounds and input
  prompts, libretro's CC-BY system icons, our own art), credited in the repo.
- iiSU's look: an option the player turns on, fetched from iiSU's public release as today.
- README screenshots retaken with the new assets from the player's own libraries, not the demo
  catalogue: the player: "you don't have to mock the games for them, you can use my own libraries
  for the shots". No account names, user names, home paths or sign-in state in any shot; game art
  credited as the stores' and publishers' in `docs/screenshots/CREDITS.md`.
- No explicit games in any README shot ("only keep them off of GitHub screenshots, you can
  honestly just audit the shots and remove the ones containing anything explicit"): every shot is
  looked over before it is committed, and one showing anything explicit is retaken without that
  game. Nothing in the product filters games for this.
