"""Command line, from the repository root:

    uv run --frozen --group sounds python -m tools.sounds [--set NAME] [--out DIR]

Renders each set's cues (or only NAME's), matches each to its category's loudness and writes it as
48 kHz mono Ogg Vorbis into the set's folder, assets/sounds/<set>/ unless its timbre names another
(or DIR/<set>/), then prints the decoded file's measures. A timbre with menu music gets its loop
written too, beside the set (or as DIR/<set>-music.ogg).
"""

import argparse
from pathlib import Path

import soundfile

from tools.checkout import REPO
from tools.sounds.loudness import Measure
from tools.sounds.music import Loop
from tools.sounds.score import CUES
from tools.sounds.synth import RATE
from tools.sounds.timbres import TIMBRES, Timbre

VORBIS_QUALITY = 0.6
PEAK_LIMIT_DBFS = -1.0
# The menu music's loudest 400 ms, well under the cues so it sits behind them.
MUSIC_LUFS = -27.0


def write_clip(clip, lufs: float, file: Path, name: str) -> Measure:
    """Writes `clip` matched to `lufs` into `file`; refuses a decoded file peaking above the
    limit."""
    clip = clip * 10.0 ** ((lufs - Measure.of(clip).lufs) / 20.0)
    soundfile.write(
        file, clip, RATE, format="OGG", subtype="VORBIS", compression_level=1.0 - VORBIS_QUALITY
    )
    decoded, _ = soundfile.read(file)
    measure = Measure.of(decoded)
    if measure.peak_dbfs > PEAK_LIMIT_DBFS:
        raise SystemExit(
            f"{name} peaks at {measure.peak_dbfs:.2f} dBFS, above {PEAK_LIMIT_DBFS} dBFS; "
            "lower its loudness"
        )
    return measure


def write_set(timbre: Timbre, out: Path) -> dict[str, Measure]:
    """Writes every cue on `timbre` into `out`."""
    out.mkdir(parents=True, exist_ok=True)
    return {
        cue.file: write_clip(
            cue.render(timbre), cue.category.value, out / cue.file, f"{timbre.name}/{cue.file}"
        )
        for cue in CUES
    }


def write_music(timbre: Timbre, file: Path) -> Measure:
    """Writes `timbre`'s menu music loop into `file`."""
    file.parent.mkdir(parents=True, exist_ok=True)
    loop = Loop().render(timbre.tonic, timbre.voice)
    return write_clip(loop, MUSIC_LUFS, file, f"{timbre.name}'s music")


def print_table(name: str, measures: dict[str, Measure]) -> None:
    print(f"{name}: file, ms, peak dBFS, LUFS (momentary max), RMS dBFS, >6 kHz %, 2-5 kHz %")
    for file, m in measures.items():
        print(
            f"  {file:22} {m.seconds * 1000:5.0f} {m.peak_dbfs:7.2f} {m.lufs:7.2f} "
            f"{m.rms_dbfs:7.2f} {m.high_share * 100:7.3f} {m.harsh_share * 100:6.2f}"
        )


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument("--set", choices=sorted(TIMBRES))
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    for name, timbre in TIMBRES.items():
        if args.set not in (None, name):
            continue
        if args.out is not None:
            folder = args.out / name
        else:
            folder = REPO / (timbre.folder or f"assets/sounds/{name}")
        measures = write_set(timbre, folder)
        if timbre.music:
            music = args.out / f"{name}-music.ogg" if args.out else REPO / timbre.music
            measures[music.name] = write_music(timbre, music)
        print_table(name, measures)
    return 0
