# Isolated Steam waits forever on Steam Runtime's launcher service

Since Steam's SteamRT3 client (Nov 2026), Steam runs `steam-runtime-launch-client --alongside-steam`
at start (`lspci -mm -n`) through `steam-runtime-launcher-service`, both on the session bus.
`steam::SecretBus` owns the launcher's names (a0cad433), so the service starts, but its
`ProcessExited` signal goes to the client's unique name and `xdg-dbus-proxy --filter` denies it
(`--log`: `C5: -> :1.N signal …Launcher1.ProcessExited … *DENIED*`). A filtered proxy lets a
client send only to names its policy names; a caller gets SEE and one reply, never TALK, so no
`--own`, `--talk` or `--broadcast` rule passes it. Steam then never signs in: the player sees
"Waiting for Steam to sign in". Shown 2026-11-19 with Slay the Spire from the Mac and, by hand,
`steam-runtime-launch-client` through the proxy (hangs) and on the session bus directly (returns).

Fix: a fork of xdg-dbus-proxy where the clients of one proxy may talk to each other's unique
names, built and shipped beside openSU's Gamescope, so Steam's own processes reach each other
while the desktop bus stays filtered to the Secret Service.
