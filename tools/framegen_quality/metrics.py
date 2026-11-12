"""How far a generated frame is from the real one: PSNR over RGB, SSIM over luma (11-tap Gaussian,
sigma 1.5, the usual constants) and NVIDIA's FLIP (LDR, 67 pixels per degree), lower is better.

Those score a frame alone and charge a missing shape less than a misplaced one, so two more see
what a generator leaves out: the detail the real frame has and the made one lacks, and, played
between its real neighbours, the detail both of them have around a place and it lacks (a shape
there in both real frames and gone from the made one flickers at half the rate)."""

import math

import flip_evaluator
import numpy as np

_PEAK = 255.0
_C1 = (0.01 * _PEAK) ** 2
_C2 = (0.03 * _PEAK) ** 2


def psnr(truth: np.ndarray, made: np.ndarray) -> float:
    mse = float(np.mean((truth - made) ** 2, dtype=np.float64))
    return math.inf if mse == 0 else 10 * math.log10(_PEAK * _PEAK / mse)


def _gaussian(sigma: float = 1.5, radius: int = 5) -> np.ndarray:
    taps = np.exp(-(np.arange(-radius, radius + 1) ** 2) / (2 * sigma * sigma))
    return taps / taps.sum()


def _blur(plane: np.ndarray, taps: np.ndarray) -> np.ndarray:
    """Separable, summed in place so a wide kernel costs no more memory than a narrow one."""
    radius = len(taps) // 2
    height, width = plane.shape
    padded = np.pad(plane.astype(np.float32, copy=False), radius, mode="reflect")
    rows = np.zeros((padded.shape[0], width), np.float32)
    for i, w in enumerate(taps):
        rows += np.float32(w) * padded[:, i : i + width]
    out = np.zeros((height, width), np.float32)
    for i, w in enumerate(taps):
        out += np.float32(w) * rows[i : i + height, :]
    return out


def _luma(rgb: np.ndarray) -> np.ndarray:
    return rgb[..., 0] * 0.299 + rgb[..., 1] * 0.587 + rgb[..., 2] * 0.114


def ssim(truth: np.ndarray, made: np.ndarray) -> float:
    x, y = _luma(truth), _luma(made)
    taps = _gaussian()
    mx, my = _blur(x, taps), _blur(y, taps)
    sxx = _blur(x * x, taps) - mx * mx
    syy = _blur(y * y, taps) - my * my
    sxy = _blur(x * y, taps) - mx * my
    index = ((2 * mx * my + _C1) * (2 * sxy + _C2)) / (
        (mx * mx + my * my + _C1) * (sxx + syy + _C2)
    )
    return float(index.mean())


def flip(truth: np.ndarray, made: np.ndarray) -> tuple[float, np.ndarray]:
    """The mean FLIP error and its per-pixel map."""
    error, mean, _ = flip_evaluator.evaluate(
        (truth / _PEAK).astype(np.float32),
        (made / _PEAK).astype(np.float32),
        "LDR",
        applyMagma=False,
    )
    return float(mean), np.asarray(error)


def _energy(rgb: np.ndarray, sigma: float) -> np.ndarray:
    """Luma gradient magnitude, spread by a Gaussian of `sigma` pixels."""
    y = _luma(rgb)
    across = np.abs(np.diff(y, axis=1, append=y[:, -1:]))
    down = np.abs(np.diff(y, axis=0, append=y[-1:, :]))
    return _blur(across + down, _gaussian(sigma, max(1, round(3 * sigma))))


def _lost(reference: np.ndarray, made: np.ndarray) -> float:
    total = float(reference.sum())
    return 0.0 if total == 0 else float(np.maximum(reference - made, 0).sum()) / total


def detail_lost(truth: np.ndarray, made: np.ndarray) -> float:
    """The share of the real frame's edge energy the made one lacks, 0 for none, 1 for all."""
    return _lost(_energy(truth, 1.5), _energy(made, 1.5))


# Wide enough that a shape moving between the real frames is still in its own place.
_PRESENCE_SIGMA = 6.0


def flicker(before: np.ndarray, after: np.ndarray, made: np.ndarray) -> float:
    """The share of the edge energy both real neighbours have around each place that the made
    frame between them lacks: 0 when nothing they both show is missing from it."""
    both = np.minimum(_energy(before, _PRESENCE_SIGMA), _energy(after, _PRESENCE_SIGMA))
    return _lost(both, _energy(made, _PRESENCE_SIGMA))
