# openSU's own default assets, iiSU's as an option, and new README screenshots

Status: open, planned; follows the UI toolkit's theme (`docs/design/ui-toolkit.md`).

iiSU's repository has no licence, so its sounds, console cards, frame glyphs and nav icons are all
rights reserved. openSU never ships them but downloads them at run time from iiSU's public
release (`artwork/starter_pack.hpp`, Alpha 7.4), which is legally shaky for a public product and
breaks for everyone if that release changes.

Decided:
- Default: openSU's own set from freely licensed sources (Kenney's CC0 UI sounds and input
  prompts, libretro's CC-BY system icons, our own art), credited in the repo.
- iiSU's look: an option the player turns on, fetched from iiSU's public release as today.
- README screenshots retaken with the new assets from the player's own libraries, not the demo
  catalogue: the player: "you don't have to mock the games for them, you can use my own libraries
  for the shots". No account names, user names, home paths or sign-in state in any shot; game art
  credited as the stores' and publishers' in `docs/screenshots/CREDITS.md`.
- No sexual content in any shot ("don't show any NSFW games there", "Violent games are fine"): the
  shot's library leaves out games with Steam's content descriptors 3 (Adult Only Sexual Content)
  and 4 (Frequent Nudity or Sexual Content), GOG's catalogue tags for nudity and sexual content
  (the player has explicit games on GOG), and every game the player hid in openSU; not age ratings
  or violence. Every frame is looked over before it is committed; a game that slips through is
  excluded by id and the shot retaken.
