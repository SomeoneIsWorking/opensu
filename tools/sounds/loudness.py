"""Measures of one clip: peak, RMS, ITU-R BS.1770 loudness and how its energy splits by band."""

from dataclasses import dataclass

import numpy as np

from tools.sounds.synth import RATE

# BS.1770 K-weighting at 48 kHz: the head shelf, then the RLB high-pass.
K_WEIGHTING = (
    (
        (1.53512485958697, -2.69169618940638, 1.19839281085285),
        (1.0, -1.69065929318241, 0.73248077421585),
    ),
    ((1.0, -2.0, 1.0), (1.0, -1.99004745483398, 0.99007225036621)),
)
MOMENTARY_WINDOW = 0.4
MOMENTARY_HOP = 0.01
# Silence after the clip, so the K-filter's tail is kept by the FFT filtering.
FILTER_PAD = 1.0
HIGH_BAND = 6_000.0
HARSH_BAND = (2_000.0, 5_000.0)


def decibels(value: float) -> float:
    return 20.0 * np.log10(max(value, 1e-12))


def _k_weighted(clip: np.ndarray) -> np.ndarray:
    length = len(clip) + round(FILTER_PAD * RATE)
    spectrum = np.fft.rfft(clip, length)
    z = np.exp(-1j * np.linspace(0.0, np.pi, len(spectrum)))
    for b, a in K_WEIGHTING:
        spectrum *= np.polyval(b[::-1], z) / np.polyval(a[::-1], z)
    return np.fft.irfft(spectrum, length)


def momentary_max(clip: np.ndarray) -> float:
    """The loudest 400 ms window (BS.1770 momentary loudness) in LUFS, the clip silent around."""
    weighted = _k_weighted(clip)
    window = round(MOMENTARY_WINDOW * RATE)
    padded = np.concatenate((np.zeros(window), weighted))
    energy = np.concatenate(([0.0], np.cumsum(padded**2)))
    hop = round(MOMENTARY_HOP * RATE)
    ends = np.arange(window, len(padded) + 1, hop)
    loudest = float(np.max(energy[ends] - energy[ends - window])) / window
    return -0.691 + 10.0 * np.log10(max(loudest, 1e-12))


def _band_share(clip: np.ndarray, low: float, high: float) -> float:
    power = np.abs(np.fft.rfft(clip)) ** 2
    frequencies = np.fft.rfftfreq(len(clip), 1.0 / RATE)
    band = (frequencies >= low) & (frequencies < high)
    return float(np.sum(power[band]) / max(np.sum(power), 1e-30))


@dataclass(frozen=True)
class Measure:
    seconds: float
    peak_dbfs: float
    rms_dbfs: float
    lufs: float
    high_share: float
    harsh_share: float

    @classmethod
    def of(cls, clip: np.ndarray) -> "Measure":
        return cls(
            seconds=len(clip) / RATE,
            peak_dbfs=decibels(float(np.max(np.abs(clip)))),
            rms_dbfs=decibels(float(np.sqrt(np.mean(clip**2)))),
            lufs=momentary_max(clip),
            high_share=_band_share(clip, HIGH_BAND, RATE / 2.0 + 1.0),
            harsh_share=_band_share(clip, *HARSH_BAND),
        )
