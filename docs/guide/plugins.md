# Lua plugins

Plugins are an integration, off until turned on in Settings › Integrations
([stores.md](stores.md#integrations)). openSU runs small Lua plugins in a sandbox: no file access
beyond the paths a plugin's `plugin.json` declares, no network beyond its declared HTTPS hosts, no
processes, and limits on memory and run time. A plugin that keeps failing is stopped; openSU
carries on.

- **Bundled**: *Eden play time* shows the play time Eden recorded on each Switch game's details
  page. It finds a game by the `[0100…]` program ID in its file name, or by its title in Eden's
  game list. Turn it off in Settings › Plugins.
- **Your own**: put a plugin's folder (`plugin.json` and `main.lua`) in
  `~/.local/share/opensu/plugins/`. It starts off; turning it on in Settings › Plugins shows what
  it can read and which hosts it can reach before it runs.

The API (version 1: play-time sources and emulator game lists) is described in
[docs/design/lua-plugins.md](../design/lua-plugins.md).
