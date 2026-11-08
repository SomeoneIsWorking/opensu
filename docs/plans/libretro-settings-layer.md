# Libretro settings layer

Requested 2026-11-07: "I want a libretro layer in game settings also need a way to get into game
settings without picking a game because maybe I wanna configure game settings for all arcade games.
And libretro layer is for all libretro games."

## What it does

- A Libretro layer between Global and each built-in core (`emulators`): what is set there applies
  to every game the runner plays (shader preset, aspect, frame generation, touch controls, run
  ahead), whichever core plays it. A core's layer and a game's own value still win over it.
- Every layer above a game is reachable without a game: Settings › Emulators lists Global,
  Libretro, Consoles (Arcade among them) and Built-in cores, each opening the same layered editor
  on that scope. The Consoles list exists (`layer_settings`); the Libretro row and its scope are
  new.

## Order

1. `settings/game_settings_store`: a `libretro` key beside `global`, `systems`, `emulators`,
   `games`; the resolve order Global, Libretro (runner games only), console, core, game.
2. `app/game_settings`: the layer's name ("Libretro") and source text ("From Libretro") from the
   same `layerName`/`sourceText` owner.
3. `app/layer_settings`: the Libretro row and its editor context; a setting's sheet gains
   the Libretro level for a runner game.
4. Proof: a shader preset set on Libretro shows on an arcade game and a SNES game, and the arcade
   game's own value overrides it.
