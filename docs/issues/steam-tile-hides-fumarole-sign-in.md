# Steam's tile says "0 games" when fumarole is not signed in

Status: open.

Reported by the player (2026-11-27, Ally, b949272a): "Why can't I use Steam here?"

On the Ally, Steam is added through fumarole (`steamKind: fumarole`), and fumarole has never
signed in (`fumarole list --json`: "not signed in; run `fumarole auth` first"). The Steam tile
reads "0 games" and A on it toasts "no games in Steam", so nothing tells the player that Steam
needs a sign-in or offers one. The Steam client's own installs there are not listed, as fumarole's
choice means.

Cause: `fumarole_installs.cpp:Installs::owned` logs the failed `fumarole list` and returns no
games, so `steam::Provider::list` reports Steam ready with nothing in it. GOG and Epic in the same
state show "Sign in" and open their sign-in.
