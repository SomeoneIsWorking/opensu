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
- Use: the existing sources already give every game art, so SteamGridDB does not replace them.
  It is for two things:
  - Art that does not fit: when a game's saved cover has a shape other than its tile's (it would
    be letterboxed or cropped), openSU fetches a SteamGridDB grid of the tile's shape (by Steam app
    id, else by title search) and shows that instead; the original stays on disk.
  - Alternatives: a per-game "Choose artwork" picker (cover, hero, logo, icon) listing SteamGridDB's
    options beside the current art; the choice is kept per game and survives refreshes.
