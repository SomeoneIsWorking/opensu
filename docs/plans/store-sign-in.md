# Store sign-in inside openSU, without Steam's browser

Requested 2026-11-25 from the Ally: "Why does epic sign in open Steam browser?", "I wanna reduce the
dependency on Steam, not increase it", "openSU needs its own browser with an option to import
cookies from other browsers (Zen in this case) so we can use google login on GoG and Epic", and
`epicgames.com/activate` "can also work".

## What is wrong today

The sign-in openSU has (`app::sign_in`, started from an Epic or GOG tile that is not signed in)
opens the store's page in the default browser through `xdg-open` and waits for a Firefox or Zen
add-on (`extension/opensu-signin`) to hand the code to the control channel. In Bazzite's game
mode the https handler is `/usr/bin/steam-http-loader`, which hands the page to Steam as
`steam://openurl_external/...` (the Ally's journal, 2026-11-25 21:33:42), where the add-on cannot
run, so the sign-in never finishes. SteamGridDB's Connect falls back to Steam's browser the same way
(`steam::SteamGridDbSignIn`).

## What it does

1. **Epic by device code.** Settings › Stores › Epic › Sign in shows Epic's activation code and a
   QR code for `epicgames.com/activate`; the player signs in on a phone (Google included, in the
   phone's own browser), openSU polls Epic's device authorization until it is granted, turns the
   grant into an exchange code and hands it to legendary (`legendary auth --token`). No browser on
   the device. First check: that the device grant's client can issue an exchange code legendary's
   client accepts; if not, step 2 carries Epic too.
2. **openSU's own browser** in place of the default browser and the add-on, for sign-ins that need
   a page on the device (GOG, Epic's fallback, SteamGridDB): a page in openSU's UI, driven by the
   pad and touch, on the platform's engine (WebKitGTK on Linux, present on Bazzite and Fedora; WKWebView on macOS; WebView2 on Windows; the
   system WebView on Android). Its profile is openSU's own. It watches for the store's redirect
   (Epic's `authorizationCode`, GOG's `on_login_success?code=`) and finishes the sign-in itself;
   nothing goes through `xdg-open`.
3. **Cookies from another browser**, on request: the player picks an installed browser (Zen's
   Flatpak profile `~/.var/app/app.zen_browser.zen/.zen`, Firefox and its forks by `cookies.sqlite`;
   Chromium-family by their keyring-sealed store later) and openSU copies only the cookies of the
   sign-in's domains (the store's and Google's) into its own profile, so Google sees a signed-in
   session instead of refusing an embedded browser. Nothing is read without that choice, and
   nothing is written back.

## Not doing

- Steam's browser or `xdg-open` for any sign-in.
- Reading another browser's cookies beyond the domains the sign-in needs.
