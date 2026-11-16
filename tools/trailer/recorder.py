"""Frames grabbed as fast as a source gives them, each with the time it was taken, and a clip
made from them at a constant frame rate.

A source gives raw RGBA (the shell's `/frame.rgba`, fast enough for motion) or PNG (the game
runner's `/frame.png`). A take holds one or two sources per frame: the shell's frame alone, or the
runner's frame with the shell's frame over it, as Gamescope composes them for the player.
"""

import subprocess
import threading
import time
from collections.abc import Callable
from dataclasses import dataclass, field
from pathlib import Path


@dataclass(frozen=True)
class Picture:
    data: bytes
    # The size of raw RGBA; 0 for PNG, which carries its own.
    width: int = 0
    height: int = 0

    @property
    def raw(self) -> bool:
        return self.width > 0


Source = Callable[[], Picture]


@dataclass
class Take:
    folder: Path
    names: tuple[str, ...]
    times: list[float] = field(default_factory=list)
    # Each source's raw size, or None for PNG.
    sizes: dict[str, tuple[int, int] | None] = field(default_factory=dict)

    def frame(self, index: int, name: str) -> Path:
        suffix = "png" if self.sizes.get(name) is None else "rgba"
        return self.folder / f"{index:05d}-{name}.{suffix}"

    @property
    def seconds(self) -> float:
        return self.times[-1] - self.times[0] if len(self.times) > 1 else 0.0


class Recorder:
    """Grabs every source in turn on a thread of its own until stopped."""

    def __init__(self, sources: dict[str, Source], folder: Path) -> None:
        folder.mkdir(parents=True, exist_ok=True)
        for old in [*folder.glob("*.png"), *folder.glob("*.rgba"), *folder.glob("*.raw")]:
            old.unlink()
        self.take = Take(folder, tuple(sources))
        self._sources = sources
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._error: BaseException | None = None

    def _run(self) -> None:
        try:
            while not self._stop.is_set():
                taken = time.monotonic()
                index = len(self.take.times)
                for name, source in self._sources.items():
                    picture = source()
                    self.take.sizes[name] = (picture.width, picture.height) if picture.raw else None
                    self.take.frame(index, name).write_bytes(picture.data)
                self.take.times.append(taken)
        except BaseException as error:  # noqa: BLE001 - raised again on the caller's thread
            self._error = error

    def __enter__(self) -> "Recorder":
        self._thread.start()
        return self

    def __exit__(self, *_exc: object) -> None:
        self._stop.set()
        self._thread.join(timeout=30)
        if self._error is not None:
            raise RuntimeError(f"recording {self.take.folder} failed") from self._error


def _ticks(take: Take, fps: int) -> list[int]:
    """For each frame of the clip, the newest grab taken by then."""
    start = take.times[0]
    chosen = []
    newest = 0
    for tick in range(max(1, round(take.seconds * fps))):
        at = start + tick / fps
        while newest + 1 < len(take.times) and take.times[newest + 1] <= at:
            newest += 1
        chosen.append(newest)
    return chosen


def _input(take: Take, name: str, ticks: list[int], fps: int) -> list[str]:
    """ffmpeg input words for one source at a constant `fps`: raw frames joined into one file, or
    a concat list of PNGs."""
    size = take.sizes[name]
    if size is not None:
        joined = take.folder / f"{name}.raw"
        with joined.open("wb") as out:
            for index in ticks:
                out.write(take.frame(index, name).read_bytes())
        width, height = size
        return ["-f", "rawvideo", "-pix_fmt", "rgba", "-s", f"{width}x{height}"] + [
            "-r",
            str(fps),
            "-i",
            str(joined),
        ]
    listing = take.folder / f"{name}.txt"
    lines = []
    for index in ticks:
        lines += [f"file '{take.frame(index, name)}'", f"duration {1 / fps:.6f}"]
    lines.append(f"file '{take.frame(ticks[-1], name)}'")
    listing.write_text("\n".join(lines) + "\n")
    return ["-f", "concat", "-safe", "0", "-i", str(listing)]


def encode(take: Take, out: Path, width: int, height: int, fps: int) -> Path:
    """The take as an H.264 clip at `fps`, each source fitted to `width` x `height` and the
    second laid over the first."""
    if not take.times:
        raise RuntimeError(f"no frames in {take.folder}")
    ticks = _ticks(take, fps)
    fit = (
        f"scale={width}:{height}:force_original_aspect_ratio=decrease:flags=lanczos,"
        f"pad={width}:{height}:(ow-iw)/2:(oh-ih)/2:color=black,setsar=1"
    )
    inputs: list[str] = []
    for name in take.names:
        inputs += _input(take, name, ticks, fps)
    if len(take.names) == 1:
        graph = f"[0:v]{fit},fps={fps},format=yuv420p[v]"
    else:
        graph = (
            f"[0:v]{fit},fps={fps}[game];"
            f"[1:v]scale={width}:{height}:flags=lanczos,fps={fps},format=rgba[ui];"
            f"[game][ui]overlay=format=auto:shortest=1,format=yuv420p[v]"
        )
    subprocess.run(
        ["ffmpeg", "-y", "-loglevel", "error", *inputs, "-filter_complex", graph]
        + ["-map", "[v]", "-c:v", "libx264", "-preset", "slow", "-crf", "14", str(out)],
        check=True,
    )
    for joined in take.folder.glob("*.raw"):
        joined.unlink()
    return out
