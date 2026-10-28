# SteamGridDB artwork, its API key got through the Steam login

Status: open; connected on the player's own account (Isolated, hidden run in the headless Gamescope
fork), the key in the keyring, fitting (games with no art included), "Choose artwork" and
Disconnect's dialog checked on the real library. Not checked: a shown (not hidden) session, the pad
pressing Steam's "Sign In", Normal mode.

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
- Which Steam, by mode:
  - Managed, Isolated: openSU's own Steam. Connect starts it in its mode when it is not up (it
    stops when Steam exits; the client owner restarts it, as a launch does) and waits until it is
    logged on (status "Starting Steam", Cancel works). That Steam runs `-silent` inside openSU's
    Gamescope and its desktop UI shows no window for `steam://openurl`, so the sign-in opens Big
    Picture, whose browser shows the page, and closes Big Picture after unless the player had it
    up. While the sign-in runs, `IntegrationSwitch` shows Steam's windows
    (GAMESCOPE_HIDE_STEAM_CLIENT 0) and does not close Big Picture; both guards come back when it
    ends. A desktop Steam keeping openSU's out (Blocked) or a Steam that does not start is an
    error naming why; the system browser does not open.
  - Normal: the desktop Steam when its DevTools answer (only when it was started with
    `-cef-enable-debugging`), in its own window; otherwise the system browser and the key field.
    Normal is the desktop launcher, so the desktop's browser is the right fallback there, and
    openSU never switches a desktop Steam to Big Picture.
  - No Steam to use (Steam off, not installed, or a hidden run that starts none): the system
    browser and a field to paste the key with the on-screen keyboard. "Enter a key" opens the same
    page in any mode.
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

- The player's report "SteamGridDB uses my own browser, not steam browser" (Isolated, journal of
  2026-10-22/23), reproduced in a hidden run with the player's Steam in the headless Gamescope
  fork (`scratch/steamgriddb-signin/drive.py`, frames from Gamescope's PipeWire stream):
  - The system browser at 21:06:25: that openSU was in Isolated with its Steam Blocked by a
    desktop Steam an earlier Normal run had started, so nothing answered on 8080 and
    `SteamGridDbAccount::perform(Connect)` took any unreachable DevTools as "no Steam" and opened
    the default browser. Fixed: own Steam is started and waited for, Blocked is an error.
  - "sign-in through Steam: cancelled" at 21:07:18, :24, :35 and 17:51:50: Steam was Ready, but
    `SteamGridDbSignIn::run` sent `steam://openurl` to the desktop UI of the `-silent` client,
    which opens no page and no window (the DevTools page list does not change), so the sign-in
    waited for a view that never came until the player pressed Cancel (Settings › Artwork ›
    Cancel, `SteamGridDbAccount::cancel`). Had a view opened, GAMESCOPE_HIDE_STEAM_CLIENT 1 kept it
    out of the composite: with the atom 1 Gamescope composited nothing, with 0 the page showed.
    Fixed: Big Picture's browser, Steam's windows shown while the sign-in runs.
  - Found on the way: "Login via Steam" was matched by /steam/i, which first hit the page's
    "Steam Pre-Greenlight Project" link; and clicking the real button makes Steam's browser open
    the OpenID page in a popup of its own, outside the view. The button is now matched by "log
    in" and "Steam" together and its link followed in the view.
  - Big Picture just after a logon sits on its login route and then goes to its home over any
    page opened meanwhile; the sign-in waits until it is past that route
    (`steam::BigPicture::shown`).
  - After the fix, both with Steam already up and with Steam stopped before Connect: Big Picture
    opens, Steam's OpenID "Sign In" page for www.steamgriddb.com shows in front, Cancel closes
    Big Picture and hides Steam again; no system browser.
  - The pads during the sign-in: in Isolated with "openSU's virtual pads" Steam's sandbox sees only
    openSU's kept virtual pads, which rest unless a game holds them, so the player's pad could not
    reach Big Picture. Fixed: `PadOwner::lendToSteam` holds the pads for Big Picture while the
    sign-in runs, by the same claim rule as a game (`serves` = Big Picture), and releases them when
    it ends. Shown in a hidden run with a scratch uinput pad linked into `hidden-pads`
    (`drive.py NAME BUILD pad`): the pad's presses came out of kept pad `event21`, which Steam had
    open; Steam's `RegisterForControllerInputMessages` reported controller 0 Up, Right, Left, Down;
    Big Picture's focus moved from the OpenID page into its top bar (search field), right to the
    next item, back, and down into the page (Gamescope frames); after Cancel the hide atom was 1
    and a press reached no virtual pad.

- Live on the player's account (2026-10-28, `scratch/steamgriddb-live/live.py`: hidden openSU
  with `--steam` in the headless Gamescope fork, the real HOME, XDG dirs and keyring, frames from
  `/frame.png` and Gamescope's PipeWire stream):
  - Settings › Artwork › Connect: openSU's Steam logged on, Big Picture opened Steam's OpenID page
    for www.steamgriddb.com (realm and `return_to` on www.steamgriddb.com) already signed in to
    the player's account; "Sign In" was pressed through DevTools, as the pad cannot reach it.
    `/login/steam` redirected to the API page, the key was read about 2 s later, Big Picture
    closed, the hide atom went back to 1, "SteamGridDB connected", "Key kept in the keyring". The
    keyring holds a 32-character secret under `name=steamgriddb-api-key`; no log or frame holds it.
    A restart reads it back ("connected, key kept in the keyring").
  - Fitting: the real API and CDN answer; Steam, GOG, Epic and ROM covers of another shape got
    square grids (ROMs and GOG/Epic by title search, e.g. both X-Men Legends II ROMs).
  - "Choose artwork" on Absolute Drift (GOG and Epic): 24 covers listed with thumbnails; a pick
    showed on the tile, survived a restart, and "Own" put the original back.
  - Found and fixed on the way:
    - One image the CDN did not deliver (`Timeout was reached`) or one API call that did not
      answer (`Could not connect to server`) ended the fitting round with the queue cleared, so
      after a few dozen games nothing more was fitted until the catalog next changed (seen in
      three runs). Now an image that does not come skips that game, and an API that does not
      answer pauses 30 s and asks that game once more at the end of the round
      (`CoverFitter::fit`, `Outcome`); only no key, a refused key or a cover that cannot be kept
      ends it.
    - A game queued again by a new catalog while its fit was under way was fetched twice; the
      worker now checks `wantsFit` when it takes the game.
    - The picker's card ran to the window's foot over its "Close"/"Choose" hints and covered the
      top bar; it is now a `ui::tallCard` with its own hint bar, as search and the folder chooser.
    - After a pick, the details page under the picker kept showing the old cover until reopened;
      `SteamGridDbCovers` now says when a tile's cover changes (`Hooks::shown`) and the shell
      reads the details page again.
    - The picker looked a game up by Steam app id or title only, the fitter also by an own game's
      matched app id; both use `steamGridDbGameOf`.
    - Running ctest while an openSU ran stopped that openSU's Steam: `OrphanedSteam` took a scope
      as orphaned when its pid was not the caller's program, which in a test binary is every live
      openSU. It now names `opensu`.
  - Second pass (`scratch/steamgriddb-live/run5`):
    - Games with no art at all: the fitter took only own games as coverless, so a ROM libretro
      has no thumbnail for (Gears of War and Marvel Ultimate Alliance on Xbox 360, Time Crisis 4)
      never got a cover. `ArtworkStore::coverless` now says a game has no art and none coming (no
      source can supply it, or its source missed within `missLifetime`); the fitter asks
      SteamGridDB by title for those, and a download that ends without a cover reaches it through
      `ArtworkDelivery::Hooks::coverMissing`. A cover still to be downloaded is not coverless. On
      start, Gears of War (Xbox 360) and Marvel Ultimate Alliance got square covers by title
      (SteamGridDB's first title match for Gears carries "Redux" art).
    - Disconnect asks first ("Disconnect SteamGridDB? openSU forgets the key; Connect signs in
      again.", the row's `confirm` through the toolkit's dialog); Cancel left the key. After a
      disconnect and the same key typed back in through Enter a key, the keyring held the same
      key and the focus went to Console art: the row pressed is gone and the navigator would
      otherwise take the nearest, Disconnect (`ArtworkSettings::service`). The status row is an
      Info row the toolkit does not focus.
    - The page under a tall card showed its own hint bar, dimmed and cut by the card's edge:
      gpu2d's `Navigator::placeHints` gave the top screen's hints to every bar on the stack. A
      screen with bars of its own now keeps them; the bars under it stay empty (picker, search,
      folder chooser).

Not checked:
- Whether the account had a key already or the sign-in pressed Generate (not logged).
- Disconnect's dialog opens with its Disconnect button focused, so A then A forgets the key; the
  toolkit's confirm dialog does that for every destructive row.
- In a real session (not hidden): openSU's own window under Big Picture, FocusFollow leaving
  Big Picture open during the sign-in, the lent virtual pads resting while the shell is in front,
  and Guide over Big Picture bringing the shell back (a hidden run follows no focus).
- Reaching the page's "Sign In" with the pad: the D-pad moves Big Picture's focus through its top
  bar, but did not focus anything in the page itself, and the left stick moved no cursor; Steam
  offers "Use joystick" there, not tried.
- Normal mode against a desktop Steam with DevTools.
