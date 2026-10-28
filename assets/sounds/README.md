# openSU's interface sounds

openSU's own UI sounds, played at the events listed in `src/audio/effect.hpp`. They are synthesized
from sine partials and filtered noise by `tools/sounds/` and contain no recorded or third-party
material. They are part of openSU and licensed under its MIT licence (`LICENSE` at the repository
root).

| File | Event |
|---|---|
| `navigation.ogg` | focus moves in a list or the grid, a press |
| `open.ogg` | a panel appears |
| `close.ogg` | a panel or page is dismissed |
| `context-menu.ogg` | the context menu or a picker opens |
| `launch.ogg` | a game launches |
| `enter.ogg` | A opens a console, launcher or collection |
| `exit.ogg` | B backs out of one |
| `section-1.ogg` … `section-12-plus.ogg` | L1/R1 to a section showing 1, 2, 3-5, 6-11 or 12+ tiles |

All are 48 kHz mono Ogg Vorbis, peaks at about -6 dBFS. Regenerate them from the repository root:

```
uv run --frozen --group sounds python -m tools.sounds
```

The audio is the same on every run; the files' bytes differ, as each Ogg stream gets a new serial
number.
