# Screenshot credits

The screenshots are frames of openSU itself (`opensu --hidden`, frames from `GET /frame.png`),
captured by `uv run --frozen python -m tools.screenshots` with no network, in a throwaway home with
a demo library. The demo library is a made-up Steam install of seven freely licensed games and three
empty ROM files. No art comes from Steam, Epic or GOG, iiSU or anyone's own library. Each game's
cover is an image of that game from Wikimedia Commons, under the licence below; openSU crops
and scales it in the tile, the details page and the blurred backdrop. The files are pinned by SHA-256
in `tools/screenshots/demo.py`. All seven covers appear in `home`;
`details` shows The Battle for Wesnoth and `search` SuperTux. `library`, `guide` and `settings` show none.

| Game | Image | Author | Licence |
| --- | --- | --- | --- |
| The Battle for Wesnoth | [The Battle for Wesnoth 1.12 title screen](https://commons.wikimedia.org/wiki/File:The_Battle_for_Wesnoth_1.12_title_screen.png) | The Battle for Wesnoth developers team | [GPL](https://www.gnu.org/licenses/gpl.html) |
| OpenTTD | [OpenTTD-15.3-en](https://commons.wikimedia.org/wiki/File:OpenTTD-15.3-en.png) | Screenshot: theki; game: OpenTTD with the OpenGFX graphics set | [GPL-2.0](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html) |
| Teeworlds | [Teeworlds Screenshot Jungle 0.6.1](https://commons.wikimedia.org/wiki/File:Teeworlds_Screenshot_Jungle_0.6.1.png) | Heinrich5991 | [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0) |
| Endless Sky | [Endless Sky 0.9.12 title screen](https://commons.wikimedia.org/wiki/File:Endless_Sky_0.9.12_title_screen.png) | Endless Sky developers | [GPL-3.0](https://www.gnu.org/licenses/gpl-3.0.html) |
| Shattered Pixel Dungeon | [Shattered Pixel Dungeon title screen v3.3.8](https://commons.wikimedia.org/wiki/File:Shattered_Pixel_Dungeon_title_screen_v3.3.8.png) | Evan Debenham | [GPL-3.0](https://www.gnu.org/licenses/gpl-3.0.html) |
| Mindustry | [The Serpulo Planet - Mindustry](https://commons.wikimedia.org/wiki/File:The_Serpulo_Planet_-_Mindustry.jpg) | Mindustry | [GPL](https://www.gnu.org/licenses/gpl.html) |
| SuperTux | [Intro (SuperTux)](https://commons.wikimedia.org/wiki/File:Intro_(SuperTux).jpg) | SuperTux Devel Team | [GPL](https://www.gnu.org/licenses/gpl.html) |

The licences are as each Commons file page states them. The screenshots that contain these
images are shared under the same terms as those images.

The ROM titles (Tobu Tobu Girl, uCity, Super Tilt Bro) are file names only; their tiles show
openSU's own fallback, so no art of theirs is in any image.

openSU's own parts of every frame:

- Cal Sans (`assets/CalSans-Regular.ttf`), SIL Open Font License 1.1 (`assets/CalSans-OFL.txt`).
- Steam, Epic Games and GOG.com logos from [Simple Icons](https://simpleicons.org), CC0 1.0
  (`assets/icons/README.md`). The marks belong to their owners.
- Everything else (tiles, console and store cards, dock letters, menus, prompts) is drawn by openSU
  (MIT, `LICENSE`).
