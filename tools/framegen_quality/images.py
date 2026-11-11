"""PNG frames as float RGB arrays (0 to 255), alpha left out."""

from pathlib import Path

import numpy as np
from PIL import Image


def load_rgb(path: Path) -> np.ndarray:
    with Image.open(path) as image:
        return np.asarray(image.convert("RGB"), dtype=np.float64)


def save_rgb(picture: np.ndarray, path: Path) -> None:
    Image.fromarray(np.clip(np.rint(picture), 0, 255).astype(np.uint8), "RGB").save(path)
