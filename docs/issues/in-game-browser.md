# A browser while playing, with shortcuts

Status: open, planned.

The player: like the Decky web browser plugin, "browser shortcuts when you wanna look something up
while playing ... we need a browser".

Decided: openSU opens web pages as one of its open things (`multitasking.md`), not an embedded
engine:
- Linux: an installed Chromium-based browser or Firefox in app/kiosk mode, with an openSU profile
  of its own under `<data>/opensu/browser`, started inside openSU's Gamescope, so it sits in the
  switcher beside the game and the game stays paused or muted behind it. Missing browser: a refusal
  naming the install command per distribution.
- Other platforms: the system's default browser.
- Shortcuts: a list in Settings (name, URL, optional `{game}` placeholder for the game in front),
  shown in the quick menu during play. Defaults: a web search, the game's wiki search, the game's
  page on its store.
- The on-screen keyboard (`on-screen-keyboard.md`) types into it.
