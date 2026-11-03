"""The voices the sets are built from. A voice takes (frequency, velocity, ring, seconds) and
returns one mono note at RATE that starts click-free and decays exponentially; `ring` scales
its decay."""

from dataclasses import dataclass

import numpy as np

RATE = 48_000
ATTACK = 0.0015
# Partials above this are left out: little energy above 6 kHz, nothing near Nyquist.
PARTIAL_CEILING = 7_000.0
LOWPASS_TAPS = 255


def times(seconds: float) -> np.ndarray:
    return np.arange(round(seconds * RATE)) / RATE


def attack(length: int, seconds: float = ATTACK) -> np.ndarray:
    """A raised-cosine rise over `seconds`, then flat."""
    ramp = min(length, round(seconds * RATE))
    envelope = np.ones(length)
    envelope[:ramp] = 0.5 - 0.5 * np.cos(np.linspace(0.0, np.pi, ramp))
    return envelope


def lowpass(signal: np.ndarray, cutoff: float) -> np.ndarray:
    """A linear-phase Blackman-windowed sinc low-pass, aligned with its input."""
    n = np.arange(LOWPASS_TAPS) - (LOWPASS_TAPS - 1) / 2
    kernel = np.sinc(2.0 * cutoff / RATE * n) * np.blackman(LOWPASS_TAPS)
    return np.convolve(signal, kernel / np.sum(kernel), mode="same")


def pitch_scale(frequency: float, reference: float) -> float:
    """Higher notes ring shorter, as a struck bar or string does."""
    return (reference / frequency) ** 0.5


@dataclass(frozen=True)
class Mode:
    """One partial of a struck body: its frequency ratio, level, decay time constant at the
    reference pitch, and how strongly a harder strike (velocity) brings it out."""

    ratio: float
    level: float
    decay: float
    brightness: float = 0.0


def modal(
    frequency: float,
    velocity: float,
    ring: float,
    seconds: float,
    modes: tuple[Mode, ...],
    reference: float,
) -> np.ndarray:
    """A sum of exponentially decaying partials, each with its own decay."""
    t = times(seconds)
    scale = pitch_scale(frequency, reference) * ring
    out = np.zeros_like(t)
    for mode in modes:
        partial = frequency * mode.ratio
        if partial > PARTIAL_CEILING:
            continue
        level = mode.level * velocity**mode.brightness
        out += level * np.sin(2.0 * np.pi * partial * t) * np.exp(-t / (mode.decay * scale))
    return out * attack(len(t))


def strike(velocity: float, seconds: float, cutoff: float, decay: float, seed: int) -> np.ndarray:
    """The thud of a mallet: a few milliseconds of seeded noise, low-passed, darker when soft."""
    t = times(seconds)
    noise = np.random.default_rng(seed).standard_normal(len(t))
    burst = lowpass(noise * np.exp(-t / decay), cutoff * (0.5 + 0.5 * velocity))
    return burst * attack(len(t), 0.001)


def pluck(
    frequency: float,
    velocity: float,
    ring: float,
    seconds: float,
    *,
    reference: float,
    body: float,
    opening: float,
    closing: float,
    detune_cents: float,
    slope: float,
) -> np.ndarray:
    """Two detuned band-limited sawtooth-like tones through a low-pass whose cutoff opens to
    `opening` times the pitch at the strike and closes back with time constant `closing`."""
    t = times(seconds)
    cutoff = frequency * (1.0 + opening * velocity * np.exp(-t / closing))
    out = np.zeros_like(t)
    for cents in (-detune_cents, detune_cents):
        tone = frequency * 2.0 ** (cents / 1200.0)
        harmonic = 1
        while harmonic * tone <= PARTIAL_CEILING:
            partial = harmonic * tone
            gain = harmonic**-slope / np.sqrt(1.0 + (partial / cutoff) ** 4)
            out += gain * np.sin(2.0 * np.pi * partial * t)
            harmonic += 1
    decay = body * pitch_scale(frequency, reference) * ring
    return 0.5 * out * np.exp(-t / decay) * attack(len(t))


def place(seconds: float, *parts: tuple[float, np.ndarray]) -> np.ndarray:
    """Each (start seconds, voice) laid into one clip `seconds` long."""
    out = np.zeros(round(seconds * RATE))
    for start, voice in parts:
        at = round(start * RATE)
        span = min(len(voice), len(out) - at)
        out[at : at + span] += voice[:span]
    return out


def fade_out(clip: np.ndarray, seconds: float) -> np.ndarray:
    """A raised-cosine fade over the clip's last `seconds`."""
    fade = min(len(clip), round(seconds * RATE))
    out = clip.copy()
    out[len(out) - fade :] *= 0.5 + 0.5 * np.cos(np.linspace(0.0, np.pi, fade))
    return out
