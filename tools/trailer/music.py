"""The trailer's music, synthesized here so it is openSU's own: a pad and an arpeggio under the
title card, drums and bass with the footage, and the pad alone again under the end card.

Needs the `sounds` dependency group (numpy, soundfile).
"""

from pathlib import Path

import numpy as np
import soundfile

RATE = 48_000
BPM = 112
BEAT = 60 / BPM
BAR = 4 * BEAT
# The title card's length and the end card's: no drums under either.
INTRO = 4.0
OUTRO = 6.0
# A minor, F, C, G: two bars each would drag, so one bar each.
CHORDS = (
    (57, 60, 64),  # A minor
    (53, 57, 60),  # F
    (48, 52, 55),  # C
    (55, 59, 62),  # G
)
ARP_ORDER = (0, 1, 2, 1, 2, 0, 1, 2)
SEED = 7


def _hz(note: float) -> float:
    return 440.0 * 2 ** ((note - 69) / 12)


def _time(length: float) -> np.ndarray:
    return np.arange(round(length * RATE)) / RATE


def _saw(freq: float, t: np.ndarray) -> np.ndarray:
    return 2 * ((t * freq) % 1.0) - 1


def _lowpass(signal: np.ndarray, cutoff: float) -> np.ndarray:
    """A one-pole low-pass, run twice for a gentler top."""
    alpha = 1 - np.exp(-2 * np.pi * cutoff / RATE)
    for _ in range(2):
        out = np.empty_like(signal)
        held = 0.0
        for index, value in enumerate(signal):
            held += alpha * (value - held)
            out[index] = held
        signal = out
    return signal


def _envelope(count: int, attack: float, release: float) -> np.ndarray:
    env = np.ones(count)
    rise = min(count, round(attack * RATE))
    fall = min(count - rise, round(release * RATE))
    env[:rise] = np.linspace(0, 1, rise, endpoint=False)
    if fall:
        env[count - fall :] = np.linspace(1, 0, fall)
    return env


def _place(track: np.ndarray, sound: np.ndarray, at: float) -> None:
    start = round(at * RATE)
    if start >= len(track):
        return
    end = min(len(track), start + len(sound))
    track[start:end] += sound[: end - start]


def _pad(chord: tuple[int, ...], length: float) -> np.ndarray:
    t = _time(length)
    voice = np.zeros_like(t)
    for note in chord:
        for detune in (-0.08, 0.0, 0.08):
            voice += _saw(_hz(note + detune), t)
        voice += 0.6 * np.sin(2 * np.pi * _hz(note - 12) * t)
    return voice * _envelope(len(t), 0.35, 0.4) / (len(chord) * 3.6)


def _pluck(note: int, length: float) -> np.ndarray:
    t = _time(length)
    tone = np.sign(np.sin(2 * np.pi * _hz(note) * t)) * 0.35 + np.sin(2 * np.pi * _hz(note) * t)
    return tone * np.exp(-t * 9) * _envelope(len(t), 0.003, 0.02)


def _bass(note: int, length: float) -> np.ndarray:
    t = _time(length)
    freq = _hz(note - 24)
    tone = np.sin(2 * np.pi * freq * t) + 0.25 * _saw(freq, t)
    return tone * np.exp(-t * 2.5) * _envelope(len(t), 0.004, 0.03)


def _kick() -> np.ndarray:
    t = _time(0.4)
    sweep = 45 + 95 * np.exp(-t * 30)
    phase = 2 * np.pi * np.cumsum(sweep) / RATE
    return np.sin(phase) * np.exp(-t * 9)


def _snare(noise: np.random.Generator) -> np.ndarray:
    t = _time(0.25)
    body = np.sin(2 * np.pi * 190 * t) * np.exp(-t * 25)
    hiss = noise.uniform(-1, 1, len(t)) * np.exp(-t * 16)
    return 0.5 * body + 0.6 * hiss


def _hat(noise: np.random.Generator, open_: bool) -> np.ndarray:
    t = _time(0.18 if open_ else 0.05)
    hiss = noise.uniform(-1, 1, len(t))
    hiss = hiss - _lowpass(hiss, 6000)
    return hiss * np.exp(-t * (18 if open_ else 70))


def render(length: float, out: Path) -> Path:
    """`length` seconds of the music as a stereo WAV at `out`."""
    noise = np.random.default_rng(SEED)
    count = round(length * RATE)
    pad, arp, bass, drums = (np.zeros(count) for _ in range(4))
    groove_end = length - OUTRO
    bar = 0
    while bar * BAR < length:
        start = bar * BAR
        chord = CHORDS[bar % len(CHORDS)]
        _place(pad, _pad(chord, BAR + 0.4), start)
        for step, order in enumerate(ARP_ORDER * 2):
            note = chord[order] + 12 + (12 if step >= 8 and bar % 2 else 0)
            _place(arp, _pluck(note, 0.3), start + step * BEAT / 4)
        if INTRO <= start < groove_end:
            for eighth in range(8):
                at = start + eighth * BEAT / 2
                root = chord[0] if eighth != 7 else chord[2]
                _place(bass, _bass(root, BEAT / 2), at)
                _place(drums, 0.35 * _hat(noise, eighth % 2 == 1), at)
            for beat in range(4):
                _place(drums, _kick(), start + beat * BEAT)
                if beat in (1, 3):
                    _place(drums, 0.8 * _snare(noise), start + beat * BEAT)
        bar += 1
    # The pad swells under the title and settles once the groove starts.
    pad_level = np.interp(_time(length), [0, INTRO, INTRO + BAR], [0.9, 0.9, 0.55])
    pad = _lowpass(pad, 1800) * pad_level
    arp = _lowpass(arp, 3500) * 0.22
    # Stereo: the arpeggio and hats spread, the rest in the middle; a short echo on the arp.
    echo = round(BEAT * 0.75 * RATE)
    arp_right = np.concatenate([np.zeros(echo), arp[:-echo]]) * 0.5 + arp * 0.6
    left = pad + arp + 0.45 * bass + 0.55 * drums
    right = pad + arp_right + 0.45 * bass + 0.55 * drums
    stereo = np.tanh(np.stack([left, right], axis=1) * 1.2)
    stereo *= 0.89 / np.max(np.abs(stereo))
    out.parent.mkdir(parents=True, exist_ok=True)
    soundfile.write(out, stereo, RATE, subtype="PCM_16")
    return out
