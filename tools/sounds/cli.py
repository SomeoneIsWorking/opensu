"""Command line: `uv run --frozen --group sounds python -m tools.sounds [--out DIR]`.

Renders every sound of the set, scales each to the same peak and writes it as 48 kHz mono Ogg
Vorbis into assets/sounds/ (or DIR). Prints each file's peak and RMS.
"""

import argparse
from pathlib import Path

import numpy as np
import soundfile

from tools.sounds.sounds import SOUNDS
from tools.sounds.synth import RATE

REPO = Path(__file__).resolve().parents[2]
PEAK_DBFS = -6.0
VORBIS_QUALITY = 0.6


def decibels(value: float) -> float:
    return 20.0 * np.log10(max(value, 1e-9))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=REPO / "assets" / "sounds")
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    for name, build in SOUNDS.items():
        clip = build()
        clip *= 10.0 ** (PEAK_DBFS / 20.0) / np.max(np.abs(clip))
        file = args.out / name
        soundfile.write(
            file, clip, RATE, format="OGG", subtype="VORBIS", compression_level=1.0 - VORBIS_QUALITY
        )
        decoded, _ = soundfile.read(file)
        peak = decibels(float(np.max(np.abs(decoded))))
        rms = decibels(float(np.sqrt(np.mean(decoded**2))))
        print(
            f"{name:22} {len(decoded) / RATE * 1000:5.0f} ms  peak {peak:6.2f} dBFS  "
            f"rms {rms:6.2f} dBFS"
        )
    return 0
