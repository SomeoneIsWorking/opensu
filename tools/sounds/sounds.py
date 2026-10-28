"""The set: one clip per UI effect, named as src/audio/effect.cpp's `shippedFile` names them.

Every note is from E major pentatonic, so any two effects that overlap stay consonant.
"""

from collections.abc import Callable

import numpy as np

from tools.sounds.synth import chime, mix, pad, swept_noise

E4, B4 = 329.63, 493.88
E5, FS5, GS5, B5 = 659.26, 739.99, 830.61, 987.77
CS6, E6, FS6, GS6, B6 = 1108.73, 1318.51, 1479.98, 1661.22, 1975.53
CASCADE = (CS6, E6, FS6, GS6, B6)
CASCADE_STEP = 0.032


def navigation() -> np.ndarray:
    return mix(0.09, (0.0, 1.0, chime(E6, 0.022, 0.09)))


def open_panel() -> np.ndarray:
    return mix(0.26, (0.0, 0.8, chime(E5, 0.06, 0.26)), (0.055, 1.0, chime(B5, 0.08, 0.2)))


def close_panel() -> np.ndarray:
    return mix(0.26, (0.0, 0.8, chime(B5, 0.06, 0.26)), (0.055, 1.0, chime(E5, 0.08, 0.2)))


def context_menu() -> np.ndarray:
    return mix(0.24, (0.0, 1.0, chime(GS5, 0.07, 0.24, glide_to=CS6, glide=0.045)))


def launch() -> np.ndarray:
    notes = (E5, GS5, B5, E6)
    parts = [(0.06 * i, 0.7, chime(note, 0.09, 0.3)) for i, note in enumerate(notes[:-1])]
    parts.append((0.18, 1.0, chime(E6, 0.2, 0.5)))
    parts.append((0.0, 0.35, pad(E4, 0.03, 0.22, 0.68)))
    parts.append((0.0, 0.2, pad(B4, 0.05, 0.2, 0.68)))
    return mix(0.68, *parts)


def enter() -> np.ndarray:
    return mix(
        0.3, (0.0, 0.9, swept_noise(700.0, 2600.0, 0.16, seed=1)), (0.1, 1.0, chime(B5, 0.07, 0.2))
    )


def exit_section() -> np.ndarray:
    return mix(
        0.3, (0.0, 0.9, swept_noise(2600.0, 700.0, 0.16, seed=2)), (0.1, 1.0, chime(E5, 0.07, 0.2))
    )


def cascade(taps: int) -> Callable[[], np.ndarray]:
    """A run of `taps` soft taps rising up the scale, one per tile wave of the section."""

    def build() -> np.ndarray:
        seconds = CASCADE_STEP * (taps - 1) + 0.12
        return mix(
            seconds,
            *((CASCADE_STEP * i, 1.0, chime(CASCADE[i], 0.03, 0.12)) for i in range(taps)),
        )

    return build


SOUNDS: dict[str, Callable[[], np.ndarray]] = {
    "navigation.ogg": navigation,
    "open.ogg": open_panel,
    "close.ogg": close_panel,
    "context-menu.ogg": context_menu,
    "launch.ogg": launch,
    "enter.ogg": enter,
    "exit.ogg": exit_section,
    "section-1.ogg": cascade(1),
    "section-2.ogg": cascade(2),
    "section-3-5.ogg": cascade(3),
    "section-6-11.ogg": cascade(4),
    "section-12-plus.ogg": cascade(5),
}
