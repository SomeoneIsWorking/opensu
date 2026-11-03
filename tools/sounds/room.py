"""A small algorithmic room: early reflections, then a Freeverb-style network of damped feedback
combs into series all-passes. Short and dark, so it only gives each cue some air."""

from dataclasses import dataclass

import numpy as np

from tools.sounds.synth import RATE, lowpass

# Freeverb's comb and all-pass lengths, in seconds (its 44.1 kHz sample counts).
COMBS = tuple(samples / 44_100 for samples in (1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617))
ALLPASSES = tuple(samples / 44_100 for samples in (556, 441, 341, 225))
ALLPASS_GAIN = 0.5
# Early reflections: (delay at size 1, seconds; gain).
REFLECTIONS = ((0.0043, 0.55), (0.0071, 0.45), (0.0113, 0.38), (0.0167, 0.3), (0.0219, 0.22))
SEND_CUTOFF = 4_500.0


def _comb(signal: np.ndarray, delay: int, feedback: float, damping: float) -> np.ndarray:
    """y[n] = x[n] + feedback * (h * y)[n - delay], h a 3-tap low-pass; one block per delay."""
    taps = np.array([(1.0 - damping) ** 2, 2.0 * damping * (1.0 - damping), damping**2])
    offset = delay + 2
    out = np.zeros(len(signal) + offset)
    for start in range(0, len(signal), delay):
        end = min(start + delay, len(signal))
        at = np.arange(start, end) + offset
        echo = (
            taps[0] * out[at - delay]
            + taps[1] * out[at - delay - 1]
            + taps[2] * out[at - delay - 2]
        )
        out[at] = signal[start:end] + feedback * echo
    return out[offset:]


def _allpass(signal: np.ndarray, delay: int, gain: float) -> np.ndarray:
    """y[n] = -g x[n] + x[n - delay] + g y[n - delay], `delay` samples per block."""
    x = np.concatenate((np.zeros(delay), signal))
    y = np.zeros(len(x))
    for start in range(delay, len(x), delay):
        end = min(start + delay, len(x))
        y[start:end] = (
            -gain * x[start:end]
            + x[start - delay : end - delay]
            + gain * y[start - delay : end - delay]
        )
    return y[delay:]


def _delayed(signal: np.ndarray, seconds: float) -> np.ndarray:
    at = round(seconds * RATE)
    return np.concatenate((np.zeros(at), signal[: len(signal) - at]))


@dataclass(frozen=True)
class Room:
    """`decay` is the RT60 in seconds, `size` scales every delay, `damping` (0-0.5) darkens each
    pass through a comb, `wet` is the reverb's level against the dry note."""

    decay: float
    size: float
    damping: float
    wet: float
    predelay: float

    def apply(self, dry: np.ndarray) -> np.ndarray:
        send = _delayed(lowpass(dry, SEND_CUTOFF), self.predelay)
        early = sum(gain * _delayed(send, delay * self.size) for delay, gain in REFLECTIONS)
        late = np.zeros_like(dry)
        for seconds in COMBS:
            delay = round(seconds * self.size * RATE)
            feedback = 10.0 ** (-3.0 * delay / (self.decay * RATE))
            late += _comb(send, delay, feedback, self.damping)
        late /= len(COMBS)
        for seconds in ALLPASSES:
            late = _allpass(late, round(seconds * self.size * RATE), ALLPASS_GAIN)
        return dry + self.wet * (0.5 * early + late)
