"""Takes openSU records itself: `POST /record` on the shell's or the game runner's control channel
writes every frame it draws, raw RGBA at a steady rate, into a named pipe that ffmpeg reads.

A take holds one or two streams: the shell alone, or the runner's game with the shell over it, as
Gamescope composes them for the player. Both clocks are the machine's monotonic one, so the
streams line up by the `startedMicros` each start answers with.
"""

import json
import os
import subprocess
import urllib.error
import urllib.request
from collections.abc import Callable, Mapping
from dataclasses import dataclass
from pathlib import Path

# Lossless with an alpha channel, and quick enough to keep up at 1080p and 60 a second.
CODEC = ["-c:v", "utvideo", "-pix_fmt", "gbrap"]


@dataclass(frozen=True)
class Stream:
    """One source's recording: a lossless file and when its first frame was drawn."""

    path: Path
    width: int
    height: int
    fps: int
    started: float
    frames: int
    repeated: int

    @property
    def seconds(self) -> float:
        return self.frames / self.fps


def _call(base: str, path: str, body: str) -> dict[str, object]:
    request = urllib.request.Request(base + path, data=body.encode())
    try:
        with urllib.request.urlopen(request, timeout=40) as response:
            return json.loads(response.read())
    except urllib.error.HTTPError as error:
        raise RuntimeError(f"{base}{path}: {error.code} {error.read().decode()}") from error


class _Running:
    """A record started on `base`, ffmpeg reading its pipe into `out`."""

    def __init__(self, base: str, folder: Path, name: str, fps: int) -> None:
        self._base = base
        self._pipe = folder / f"{name}.fifo"
        self._pipe.unlink(missing_ok=True)
        os.mkfifo(self._pipe)
        self.out = folder / f"{name}.mkv"
        # The start answers before openSU opens the pipe, so ffmpeg can follow with its size.
        started = _call(base, "/record", f"{fps} {self._pipe.resolve()}")
        self._started = started
        size = f"{started['width']}x{started['height']}"
        self._encoder = subprocess.Popen(
            ["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgba"]
            + ["-s", size, "-r", str(fps), "-i", str(self._pipe), *CODEC, str(self.out)]
        )

    def stop(self) -> Stream:
        totals = _call(self._base, "/record/stop", "")
        code = self._encoder.wait(timeout=120)
        self._pipe.unlink(missing_ok=True)
        if totals.get("error"):
            raise RuntimeError(f"{self._base} stopped recording early: {totals['error']}")
        if code != 0:
            raise RuntimeError(f"ffmpeg failed on {self.out} ({code})")
        started = self._started
        return Stream(
            self.out,
            int(started["width"]),
            int(started["height"]),
            int(started["fps"]),
            int(started["startedMicros"]) / 1e6,
            int(totals["frames"]),
            int(totals["repeated"]),
        )


class Recorder:
    """Records every source from entry to exit; `streams` holds them after."""

    def __init__(self, sources: Mapping[str, Callable[[], str]], folder: Path, fps: int) -> None:
        folder.mkdir(parents=True, exist_ok=True)
        self._sources = sources
        self._folder = folder
        self._fps = fps
        self._running: list[_Running] = []
        self.streams: list[Stream] = []

    def __enter__(self) -> "Recorder":
        try:
            for name, base in self._sources.items():
                self._running.append(_Running(base(), self._folder, name, self._fps))
        except BaseException:
            self.__exit__()
            raise
        return self

    def __exit__(self, *_exc: object) -> None:
        self.streams = [running.stop() for running in self._running]
        self._running = []


def encode(streams: list[Stream], out: Path, width: int, height: int, fps: int) -> float:
    """The take as an H.264 clip at `fps`: the streams lined up from the latest start to the
    earliest end, each fitted to `width` x `height`, the second laid over the first. Answers the
    clip's length."""
    if not streams:
        raise RuntimeError(f"no streams for {out}")
    begin = max(stream.started for stream in streams)
    end = min(stream.started + stream.seconds for stream in streams)
    if end <= begin:
        raise RuntimeError(f"the streams for {out} never overlap")
    inputs: list[str] = []
    for stream in streams:
        inputs += ["-ss", f"{begin - stream.started:.4f}", "-i", str(stream.path)]
    fit = (
        f"scale={width}:{height}:force_original_aspect_ratio=decrease:flags=lanczos,"
        f"pad={width}:{height}:(ow-iw)/2:(oh-ih)/2:color=black,setsar=1"
    )
    if len(streams) == 1:
        graph = f"[0:v]{fit},fps={fps},format=yuv420p[v]"
    else:
        graph = (
            f"[0:v]{fit},fps={fps}[game];"
            f"[1:v]scale={width}:{height}:flags=lanczos,fps={fps},format=rgba[ui];"
            f"[game][ui]overlay=format=auto:shortest=1,format=yuv420p[v]"
        )
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["ffmpeg", "-y", "-loglevel", "error", *inputs, "-filter_complex", graph]
        + ["-map", "[v]", "-t", f"{end - begin:.4f}"]
        + ["-c:v", "libx264", "-preset", "slow", "-crf", "14", str(out)],
        check=True,
    )
    for stream in streams:
        stream.path.unlink()
    return end - begin
