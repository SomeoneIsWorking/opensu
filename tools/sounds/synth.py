"""The voices the set is built from: a soft chime and a swept band of noise, both mono floats."""

import numpy as np

RATE = 48_000
# Partial ratios and levels of the chime: a soft, mostly sine bell.
PARTIALS = ((1.0, 1.0), (2.0, 0.16), (3.0, 0.05), (4.0, 0.025))
ATTACK = 0.004
TAIL_FADE = 0.012


def silence(seconds: float) -> np.ndarray:
    return np.zeros(round(seconds * RATE))


def _attack(length: int) -> np.ndarray:
    ramp = min(length, round(ATTACK * RATE))
    envelope = np.ones(length)
    envelope[:ramp] = 0.5 - 0.5 * np.cos(np.linspace(0.0, np.pi, ramp))
    return envelope


def chime(
    frequency: float,
    decay: float,
    seconds: float,
    glide_to: float | None = None,
    glide: float = 0.0,
) -> np.ndarray:
    """A bell note decaying with time constant `decay`; optionally gliding to `glide_to`."""
    t = np.arange(round(seconds * RATE)) / RATE
    pitch = np.full_like(t, frequency)
    if glide_to is not None and glide > 0.0:
        progress = np.clip(t / glide, 0.0, 1.0)
        pitch = frequency * (glide_to / frequency) ** (0.5 - 0.5 * np.cos(np.pi * progress))
    phase = 2.0 * np.pi * np.cumsum(pitch) / RATE
    out = np.zeros_like(t)
    for ratio, level in PARTIALS:
        # Upper partials fade faster, so the note mellows as it rings.
        out += level * np.sin(ratio * phase) * np.exp(-t * ratio / decay)
    return out * _attack(len(t))


def pad(frequency: float, rise: float, decay: float, seconds: float) -> np.ndarray:
    """A sine that swells in over `rise` and fades with time constant `decay`."""
    t = np.arange(round(seconds * RATE)) / RATE
    swell = np.clip(t / rise, 0.0, 1.0)
    envelope = (0.5 - 0.5 * np.cos(np.pi * swell)) * np.exp(-np.maximum(t - rise, 0.0) / decay)
    return np.sin(2.0 * np.pi * frequency * t) * envelope


def swept_noise(start: float, end: float, seconds: float, seed: int) -> np.ndarray:
    """White noise through a band-pass whose centre moves from `start` to `end` Hz, swelling in
    and out over its length."""
    length = round(seconds * RATE)
    noise = np.random.default_rng(seed).uniform(-1.0, 1.0, length)
    centres = start * (end / start) ** np.linspace(0.0, 1.0, length)
    tune = 2.0 * np.sin(np.pi * centres / RATE)
    damping = 1.2
    low = band = 0.0
    out = np.empty(length)
    for i in range(length):
        high = noise[i] - low - damping * band
        band += tune[i] * high
        low += tune[i] * band
        out[i] = band
    envelope = np.sin(np.pi * np.linspace(0.0, 1.0, length)) ** 2
    return out * envelope


def mix(seconds: float, *parts: tuple[float, float, np.ndarray]) -> np.ndarray:
    """Each (start seconds, gain, voice) laid into one clip `seconds` long, the tail faded out."""
    out = silence(seconds)
    for start, gain, voice in parts:
        at = round(start * RATE)
        span = min(len(voice), len(out) - at)
        out[at : at + span] += gain * voice[:span]
    fade = round(TAIL_FADE * RATE)
    out[-fade:] *= np.linspace(1.0, 0.0, fade)
    return out
