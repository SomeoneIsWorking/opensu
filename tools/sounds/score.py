"""The cues: one per file `src/audio/effect.cpp`'s `shippedFile` names, written in degrees of a
major pentatonic scale on the timbre's tonic, so any two cues that overlap stay consonant.

Opening and entering rise from the tonic, closing and exiting fall back to it, navigation is one
soft damped note on the fifth, and the section cues run up the scale from that fifth.
"""

from dataclasses import dataclass
from enum import Enum

import numpy as np

from tools.sounds.synth import fade_out, place
from tools.sounds.timbres import Timbre

PENTATONIC = (0, 2, 4, 7, 9)
FADE_SHARE = 0.25
FADE_LIMIT = 0.12


class Category(Enum):
    """Cues of one category are matched to one momentary loudness, in LUFS."""

    NAVIGATION = -29.0
    SECTION = -25.0
    PANEL = -21.0
    PLACE = -20.0
    LAUNCH = -18.0


@dataclass(frozen=True)
class Note:
    degree: int
    start: float
    velocity: float
    ring: float = 1.0


@dataclass(frozen=True)
class Cue:
    file: str
    category: Category
    seconds: float
    notes: tuple[Note, ...]

    def render(self, timbre: Timbre) -> np.ndarray:
        """The cue's notes on `timbre` through its room, faded over its last quarter."""
        dry = place(
            self.seconds,
            *(
                (
                    note.start,
                    timbre.voice(
                        pitch(timbre.tonic, note.degree),
                        note.velocity,
                        note.ring,
                        self.seconds - note.start,
                    ),
                )
                for note in self.notes
            ),
        )
        return fade_out(timbre.room.apply(dry), min(FADE_LIMIT, FADE_SHARE * self.seconds))


def pitch(tonic: float, degree: int) -> float:
    octave, step = divmod(degree, len(PENTATONIC))
    return tonic * 2.0 ** ((12 * octave + PENTATONIC[step]) / 12.0)


def _sweep(
    degrees: tuple[int, ...], step: float, velocities: tuple[float, ...]
) -> tuple[Note, ...]:
    return tuple(
        Note(degree, i * step, velocity)
        for i, (degree, velocity) in enumerate(zip(degrees, velocities, strict=True))
    )


SECTION_STEP = 0.038
SECTION_RUN = (3, 4, 5, 6, 7)
SECTION_RING = 0.4
SECTION_TAIL = 0.2


def _section(file: str, taps: int) -> Cue:
    """A run of `taps` light taps up the scale, one per tile wave of the section."""
    notes = tuple(
        Note(SECTION_RUN[i], i * SECTION_STEP, 0.5 + 0.06 * i, SECTION_RING) for i in range(taps)
    )
    return Cue(file, Category.SECTION, SECTION_STEP * (taps - 1) + SECTION_TAIL, notes)


LAUNCH_CHORD_AT = 0.25
LAUNCH_RING = 1.8

CUES = (
    Cue("navigation.ogg", Category.NAVIGATION, 0.075, (Note(3, 0.0, 0.35, 0.18),)),
    Cue("open.ogg", Category.PANEL, 0.42, (Note(0, 0.0, 0.65), Note(3, 0.065, 0.8))),
    Cue("close.ogg", Category.PANEL, 0.42, (Note(3, 0.0, 0.7), Note(0, 0.065, 0.65))),
    Cue(
        "context-menu.ogg",
        Category.PANEL,
        0.36,
        (Note(2, 0.0, 0.6, 0.8), Note(4, 0.022, 0.7, 0.8)),
    ),
    Cue(
        "launch.ogg",
        Category.LAUNCH,
        0.95,
        _sweep((0, 2, 3, 5), 0.05, (0.55, 0.62, 0.7, 0.78))
        + (
            Note(-5, LAUNCH_CHORD_AT, 0.55, LAUNCH_RING),
            Note(0, LAUNCH_CHORD_AT, 0.5, LAUNCH_RING),
            Note(5, LAUNCH_CHORD_AT, 0.85, LAUNCH_RING),
            Note(7, LAUNCH_CHORD_AT, 0.65, LAUNCH_RING),
            Note(8, LAUNCH_CHORD_AT, 0.55, LAUNCH_RING),
        ),
    ),
    Cue(
        "enter.ogg",
        Category.PLACE,
        0.55,
        (Note(-5, 0.0, 0.5),) + _sweep((0, 2, 3, 5), 0.045, (0.6, 0.68, 0.76, 0.85)),
    ),
    Cue(
        "exit.ogg",
        Category.PLACE,
        0.55,
        _sweep((5, 3, 2, 0), 0.045, (0.8, 0.72, 0.66, 0.6)) + (Note(-5, 0.135, 0.5),),
    ),
    _section("section-1.ogg", 1),
    _section("section-2.ogg", 2),
    _section("section-3-5.ogg", 3),
    _section("section-6-11.ogg", 4),
    _section("section-12-plus.ogg", 5),
)
