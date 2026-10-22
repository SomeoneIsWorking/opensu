# SteamGridDB artwork, its API key got through the Steam login

Status: open, step 1 started.

The player: "add SteamGridDB but automate the API key process by using Steam login".

SteamGridDB has no OAuth or key endpoint: an account signs in only with Steam (OpenID) and makes
its API key on `steamgriddb.com/profile/preferences/api`. Every API call carries it as a bearer
token.

Decided:
- Getting the key: Settings › Artwork › "Connect SteamGridDB". openSU opens SteamGridDB's sign-in
  in Steam's own browser through the DevTools channel it already drives (`steam/devtools`), so the
  Steam login the client already holds signs in; the player only confirms Steam's "Sign in" page.
  openSU then opens the API page in that view, makes the key if there is none, reads it, and closes
  the view. No Steam credential, cookie or token is read by openSU; only the SteamGridDB key.
- Steam not available (not installed, not running, or not logged in): the same page in the system
  browser and a field to paste the key with the on-screen keyboard.
- The key is a secret: kept in the Secret Service (else a 0600 file under the config dir), never
  logged or shown in full; "Disconnect" forgets it.
- Use: with a key, SteamGridDB is an artwork source for every game: covers (grids), heroes, logos,
  icons. Steam games keep Steam's CDN first; ROMs, GOG, Epic and non-Steam games try SteamGridDB
  before the current sources' fallbacks, matched by Steam app id, else by title search. A per-game
  "Choose artwork" picker is a later step.
