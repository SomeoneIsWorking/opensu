"""The menu music a theme may ship: a calm loop of soft electric-piano chords over a slow pad, a
few bell notes above, in the timbre's key. Ten bars at 75 bpm, 32 s; what rings past the end is
folded onto the start, so the loop joins without a seam."""

from dataclasses import dataclass

import numpy as np

from tools.sounds.room import Room
from tools.sounds.synth import RATE, Mode, attack, lowpass, modal, place, times
from tools.sounds.timbres import Voice

TEMPO = 75.0
BEAT = 60.0 / TEMPO
BAR = 4 * BEAT
# Rings on past the last bar; folded back onto the start.
TAIL = 4.0

# Chords as semitones over the key's tonic an octave down, one per bar.
CHORDS = (
    (0, 4, 7, 11, 14),  # I maj9
    (-3, 2, 6, 9, 13),  # vi9
    (-5, 2, 7, 11, 14),  # IV maj9
    (-3, 2, 4, 7, 9),  # V6/9 over its fifth
    (0, 4, 7, 11, 14),  # I maj9
    (-8, 1, 4, 9, 13),  # iii7
    (-5, 2, 7, 11, 14),  # IV maj9
    (-3, 2, 4, 9, 11),  # V sus
    (-10, 2, 5, 7, 10),  # ii7
    (-3, 1, 4, 7, 9),  # V13
)
# The chord is struck on beat one and softly again on beat three.
STRIKES = ((0.0, 0.62), (2.0, 0.42))
# A soft electric piano: the tine, a faint octave and a short bark.
PIANO_MODES = (
    Mode(1.0, 1.0, 1.4),
    Mode(2.0, 0.22, 0.5, brightness=1.0),
    Mode(3.0, 0.06, 0.18, brightness=1.5),
)
PIANO_ATTACK = 0.006
PAD_LEVEL = 0.16
PAD_RISE = 1.2
PAD_FALL = 1.6
PAD_DETUNE_CENTS = 6.0
PAD_CUTOFF = 1_800.0
# The bell's notes above: (bar, beat, semitones over the tonic).
MELODY = (
    (0, 1.0, 14),
    (0, 2.5, 16),
    (1, 0.5, 9),
    (2, 1.0, 19),
    (2, 3.0, 16),
    (3, 2.0, 14),
    (4, 1.0, 21),
    (4, 2.5, 19),
    (5, 1.5, 16),
    (6, 0.5, 14),
    (6, 2.0, 19),
    (7, 3.0, 21),
    (8, 1.0, 16),
    (9, 2.0, 14),
)
BELL_LEVEL = 0.32
ROOM = Room(decay=1.4, size=1.0, damping=0.4, wet=0.32, predelay=0.02)


def _note(tonic: float, semitones: int) -> float:
    return tonic * 2.0 ** (semitones / 12.0)


def _piano(frequency: float, velocity: float, seconds: float, tonic: float) -> np.ndarray:
    tone = modal(frequency, velocity, 1.0, seconds, PIANO_MODES, tonic)
    return tone * attack(len(tone), PIANO_ATTACK)


def _pad(frequencies: list[float], seconds: float) -> np.ndarray:
    """Detuned sine pairs swelling in over the bar and fading after it, darkened."""
    t = times(seconds + PAD_FALL)
    out = np.zeros_like(t)
    for frequency in frequencies:
        for cents in (-PAD_DETUNE_CENTS, PAD_DETUNE_CENTS):
            out += np.sin(2.0 * np.pi * frequency * 2.0 ** (cents / 1200.0) * t)
    rise = np.clip(t / PAD_RISE, 0.0, 1.0)
    fall = np.clip((seconds + PAD_FALL - t) / PAD_FALL, 0.0, 1.0)
    envelope = np.sin(0.5 * np.pi * rise) * np.sin(0.5 * np.pi * fall)
    return lowpass(out * envelope, PAD_CUTOFF) / (2 * len(frequencies))


@dataclass(frozen=True)
class Loop:
    seconds: float = len(CHORDS) * BAR

    def render(self, tonic: float, bell: Voice) -> np.ndarray:
        """The loop in the key of `tonic`, its bell notes on `bell`, seamless at its length."""
        low = tonic / 2.0
        parts = []
        for bar, chord in enumerate(CHORDS):
            start = bar * BAR
            frequencies = [_note(low, semitones) for semitones in chord]
            parts.append((start, PAD_LEVEL * _pad(frequencies, BAR)))
            for beat, velocity in STRIKES:
                for i, frequency in enumerate(frequencies):
                    # A rolled chord, the lowest first.
                    at = start + beat * BEAT + 0.012 * i
                    parts.append((at, velocity * _piano(frequency, velocity, BAR + TAIL, low)))
        for bar, beat, semitones in MELODY:
            at = bar * BAR + beat * BEAT
            parts.append((at, BELL_LEVEL * bell(_note(tonic, semitones), 0.5, 2.5, TAIL)))
        whole = ROOM.apply(place(self.seconds + TAIL, *parts))
        length = round(self.seconds * RATE)
        loop = whole[:length].copy()
        loop[: len(whole) - length] += whole[length:]
        return loop
