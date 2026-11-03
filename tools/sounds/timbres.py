"""The candidate sets' instruments: a voice, a tonic and a room each, all playing one score."""

from collections.abc import Callable
from dataclasses import dataclass
from functools import partial

import numpy as np

from tools.sounds.room import Room
from tools.sounds.synth import Mode, modal, pluck, strike

Voice = Callable[[float, float, float, float], np.ndarray]

C5, E5, A4 = 523.25, 659.26, 440.0

# A tuned marimba bar: fundamental, the 4:1 tuned overtone and the bar's tenth-ish mode.
MARIMBA_MODES = (
    Mode(1.0, 1.0, 0.30),
    Mode(4.0, 0.28, 0.055, brightness=1.3),
    Mode(9.9, 0.05, 0.016, brightness=2.2),
)
MALLET_SEED = 1

# A struck glass: a fundamental with a slowly beating twin, a soft octave and two short
# inharmonic rim modes for the "tink".
GLASS_MODES = (
    Mode(1.0, 1.0, 0.42),
    Mode(1.0042, 0.5, 0.36),
    Mode(2.0, 0.16, 0.2, brightness=1.0),
    Mode(2.76, 0.07, 0.05, brightness=1.6),
    Mode(5.4, 0.025, 0.018, brightness=2.0),
)


def marimba(frequency: float, velocity: float, ring: float, seconds: float) -> np.ndarray:
    bar = modal(frequency, velocity, ring, seconds, MARIMBA_MODES, C5)
    mallet = strike(velocity, seconds, cutoff=2_400.0, decay=0.0012, seed=MALLET_SEED)
    return bar + 0.035 * velocity * mallet


def glass(frequency: float, velocity: float, ring: float, seconds: float) -> np.ndarray:
    return modal(frequency, velocity, ring, seconds, GLASS_MODES, E5)


soft_pluck: Voice = partial(
    pluck, reference=A4, body=0.2, opening=3.5, closing=0.03, detune_cents=5.0, slope=1.2
)


@dataclass(frozen=True)
class Timbre:
    name: str
    summary: str
    tonic: float
    voice: Voice
    room: Room


TIMBRES = {
    timbre.name: timbre
    for timbre in (
        Timbre(
            "marimba",
            "Warm wooden mallet bars in C major pentatonic, a soft felt thud and a small dry room.",
            C5,
            marimba,
            Room(decay=0.22, size=0.6, damping=0.45, wet=0.16, predelay=0.004),
        ),
        Timbre(
            "glass",
            "Struck glass in E major pentatonic: a beating, bell-like fundamental, a bright rim "
            "tink that fades fast, and a longer, airier plate.",
            E5,
            glass,
            Room(decay=0.3, size=0.85, damping=0.25, wet=0.24, predelay=0.008),
        ),
        Timbre(
            "pluck",
            "A soft synth pluck in A major pentatonic: two detuned saw tones through a low-pass "
            "that snaps open and closes, rounder and lower, in a medium room.",
            A4,
            soft_pluck,
            Room(decay=0.26, size=0.75, damping=0.4, wet=0.18, predelay=0.006),
        ),
    )
}
