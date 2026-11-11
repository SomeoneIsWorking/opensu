"""How far a generated frame is from the real one: PSNR over RGB, SSIM over luma (11-tap Gaussian,
sigma 1.5, the usual constants) and NVIDIA's FLIP (LDR, 67 pixels per degree), lower is better."""

import math

import flip_evaluator
import numpy as np

_PEAK = 255.0
_C1 = (0.01 * _PEAK) ** 2
_C2 = (0.03 * _PEAK) ** 2


def psnr(truth: np.ndarray, made: np.ndarray) -> float:
    mse = float(np.mean((truth - made) ** 2))
    return math.inf if mse == 0 else 10 * math.log10(_PEAK * _PEAK / mse)


def _gaussian(sigma: float = 1.5, radius: int = 5) -> np.ndarray:
    taps = np.exp(-(np.arange(-radius, radius + 1) ** 2) / (2 * sigma * sigma))
    return taps / taps.sum()


def _blur(plane: np.ndarray, taps: np.ndarray) -> np.ndarray:
    radius = len(taps) // 2
    padded = np.pad(plane, radius, mode="reflect")
    rows = sum(w * padded[:, i : i + plane.shape[1]] for i, w in enumerate(taps))
    return sum(w * rows[i : i + plane.shape[0], :] for i, w in enumerate(taps))


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
