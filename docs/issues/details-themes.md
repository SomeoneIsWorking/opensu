# Details page themes: Cover (today) and Steam-style hero and logo

Status: open, planned.

The player: "Currently we show cover in details, maybe we can add themes where logo and hero show
like Steam". Which art each theme uses is still to be settled ("Hero/Cover/Logo TBD").

Decided so far:
- The game details page gets a theme setting. "Cover" is today's page, the default.
- "Hero": the game's hero image fills the top of the page behind its logo, as Steam's library page
  does; the title text shows when there is no logo, and the page falls back to Cover when there is
  no hero.
- Art for it: Steam games from Steam's CDN (`library_hero`, `logo`); other games from SteamGridDB
  when connected (`steamgriddb.md`), chosen with the same per-game picker.
