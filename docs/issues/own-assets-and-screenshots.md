# openSU's own default assets, iiSU's as an option, and new README screenshots

Status: open, planned; follows the UI toolkit's theme (`docs/design/ui-toolkit.md`).

iiSU's repository has no licence, so its sounds and nav icons are all rights reserved. openSU never
ships or downloads them: downloading from iiSU's public release is legally shaky for a public
product and breaks for everyone if that release changes.

Done: console cards and tab glyphs come from a console art theme (Settings › Artwork › Console art
theme, `assets/consoles/README.md`): Photos, public-domain photos of the hardware, by default;
Drawings, openSU's own (MIT); iiSU's, read from iiSU's release APK or starter pack that the player
downloads and picks on the chooser (openSU does not download them); and any theme folder the player
puts in `<data dir>/themes/`. A theme never borrows another's art. Shown in the README's "Console
art themes" section. README shots are from the maintainer's own library (`tools.screenshots
--profile real`, `--console-theme` for each theme).

Done: the UI sounds are openSU's own (`assets/sounds/`, MIT), synthesized by `tools/sounds/`, one
per effect. openSU no longer downloads iiSU's sounds: the APK sound jobs, `artwork/iisu_assets.*`,
the store's `sound/`, `ApkArchive::overHttp`, the pinned release URL and `WebClient::getRange` are
gone. iiSU's sounds are the option (Settings › Audio › Interface sounds), read from the same iiSU
file as the console art, or a release picked there.

Decided:
- Default: openSU's own set: our own art and our own synthesized sounds.
- iiSU's look and sounds: an option the player turns on, from a copy of iiSU's release the player
  downloads and picks; the player: "users can manually download and place them somewhere".
- README screenshots retaken with the new assets from the player's own libraries, not the demo
  catalogue: the player: "you don't have to mock the games for them, you can use my own libraries
  for the shots". No account names, user names, home paths or sign-in state in any shot; game art
  credited as the stores' and publishers' in `docs/screenshots/CREDITS.md`.
- No explicit games in any README shot ("only keep them off of GitHub screenshots, you can
  honestly just audit the shots and remove the ones containing anything explicit"): every shot is
  looked over before it is committed, and one showing anything explicit is retaken without that
  game. Nothing in the product filters games for this.
