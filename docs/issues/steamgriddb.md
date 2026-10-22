# SteamGridDB artwork, its API key got through the Steam login

Status: open; built and checked against stand-ins, not yet against the real site, API or a real
Steam client.

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
  - Alternatives: a per-game "Choose artwork" picker listing SteamGridDB's covers beside the
    current one; the choice is kept per game and survives refreshes. Covers only for now, since
    the cover is what openSU shows; heroes and logos join the picker with the Steam-style details
    theme (`details-themes.md`).

Built:
- `steam/steamgriddb_sign_in` (the Steam browser flow over DevTools), `host/secret_store` (Secret
  Service through libsecret when built with it, else a 0600 file), `app/steamgriddb_account` and
  Settings › Artwork (`app/artwork_settings`): Connect, Enter a key, Cancel, Disconnect.
- `artwork/steamgriddb` (API v2, bearer to the API origin only), `artwork/cover_overrides`,
  `artwork/cover_fitter` (Home's tiles are square, so any non-square cover is fitted),
  `artwork/cover_offers`, `app/steamgriddb_covers`, "Choose artwork" on the details page
  (`ui/cover_picker*`, `app/cover_picker_controller`).

Checked:
- Unit: the DevTools flow against `tests/support/fake_steam.hpp` with browser pages (sign-in,
  generate, read, window closed by the player, cancel, time limit, no Steam); the API, fitting
  and the picker against `tests/support/steamgriddb_server.hpp`; the secret file's modes.
- Hidden run (`opensu --hidden`, offline, `OPENSU_STEAMGRIDDB_API` at
  `steamgriddb_api_stand_in`, `OPENSU_SECRET_STORE=file`): a key typed in Settings › Artwork,
  six demo covers replaced by square grids, the picker open, a pick shown on the details page.

Not checked:
- The real SteamGridDB site: the "Login via Steam" button, the API page's key field and
  Generate button are matched by text and shape, not seen.
- That `steam://openurl` makes a DevTools page in a real Steam client, and the real OpenID step.
- The real API's answers, and the libsecret keyring at run time.
