# openSU's interface sounds

openSU's own UI sounds, played at the events listed in `src/audio/effect.hpp`. There are three
sets, one folder each, chosen in Settings › Audio › Interface sounds (`settings::SoundSet`):
`marimba/` (the default), `glass/` and `pluck/`. Each holds every file below. They are synthesized
by `tools/sounds/` and contain no recorded or third-party material. They are part of openSU and
licensed under its MIT licence (`LICENSE` at the repository root).

| File | Event | Score |
|---|---|---|
| `navigation.ogg` | focus moves in a list or the grid, a press | one soft, damped note on the fifth, 75 ms |
| `open.ogg` | a panel appears | tonic up to the fifth |
| `close.ogg` | a panel or page is dismissed | fifth down to the tonic |
| `context-menu.ogg` | the context menu or a picker opens | the third and sixth, a quick strum upward |
| `launch.ogg` | a game launches | an arpeggio up to the octave into a ringing chord over the low tonic, 950 ms |
| `enter.ogg` | A opens a console, launcher or collection | tonic, third, fifth, octave rising over the low tonic |
| `exit.ogg` | B backs out of one | the same run falling, landing on the low tonic |
| `section-1.ogg` … `section-12-plus.ogg` | L1/R1 to a section showing 1, 2, 3-5, 6-11 or 12+ tiles | 1 to 5 light taps up the scale from the fifth, 38 ms apart |

## Design

- One score (`tools/sounds/score.py`) in scale degrees of a major pentatonic scale, so any two cues
  that overlap are consonant. Opening and entering rise, closing and exiting fall.
- One instrument per set (`tools/sounds/timbres.py`, its name the folder's): several partials, each with its own
  exponential decay, a 1.5 ms raised-cosine attack, and a small synthesized room
  (`tools/sounds/room.py`: early reflections, damped combs, all-passes; RT60 under 300 ms).
  Partials above 7 kHz are left out, so energy above 6 kHz stays under 0.1%.
  - `marimba`: tuned wooden bar (1 : 4 : 9.9) with a felt mallet thud, C major pentatonic.
  - `glass`: struck glass, a slowly beating fundamental with a fast rim tink, E major pentatonic.
  - `pluck`: two detuned saw tones through a closing low-pass, A major pentatonic.
  - `channels`: a soft bell, a sine and its octave with a faint upper partial and a fast decay,
    D major pentatonic. It is the Channels interface theme's own set, written into
    `assets/interface-themes/channels/sounds/` with the theme's menu music beside it
    (`music.ogg`, `tools/sounds/music.py`: ten bars of soft electric-piano chords over a pad at
    75 bpm, a few bell notes above, 32 s, what rings past the end folded onto the start so it loops
    without a seam; its loudest 400 ms at -27 LUFS, under the cues).
- Loudness is matched by category on the loudest 400 ms window (BS.1770 momentary): navigation
  -29 LUFS, section cues -25, panels -21, enter and exit -20, launch -18. Decoded peaks must stay
  below -1 dBFS or the generator refuses the set.

All are 48 kHz mono Ogg Vorbis. Regenerate every set, or one, from the repository root:

```
uv run --frozen --group sounds python -m tools.sounds
uv run --frozen --group sounds python -m tools.sounds --set glass
uv run --frozen --group sounds python -m tools.sounds --set channels
```

The audio is the same on every run; the files' bytes differ, as each Ogg stream gets a new serial
number.
