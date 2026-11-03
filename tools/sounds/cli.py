"""Command line, from the repository root:

    uv run --frozen --group sounds python -m tools.sounds --set NAME [--out DIR]
    uv run --frozen --group sounds python -m tools.sounds --audition DIR

Renders a set's cues, matches each to its category's loudness and writes it as 48 kHz mono Ogg
Vorbis into assets/sounds/ (or DIR), then prints the decoded file's measures. `--audition` writes
every set to DIR/<set>/ and one page, DIR/audition.html, to compare them on.
"""

import argparse
from pathlib import Path

import soundfile

from tools.sounds.audition import write_page
from tools.sounds.loudness import Measure
from tools.sounds.score import CUES
from tools.sounds.synth import RATE
from tools.sounds.timbres import TIMBRES, Timbre

REPO = Path(__file__).resolve().parents[2]
VORBIS_QUALITY = 0.6
PEAK_LIMIT_DBFS = -1.0


def write_set(timbre: Timbre, out: Path) -> dict[str, Measure]:
    """Writes every cue on `timbre` into `out`; refuses a decoded file peaking above the limit."""
    out.mkdir(parents=True, exist_ok=True)
    measures = {}
    for cue in CUES:
        clip = cue.render(timbre)
        clip *= 10.0 ** ((cue.category.value - Measure.of(clip).lufs) / 20.0)
        file = out / cue.file
        soundfile.write(
            file, clip, RATE, format="OGG", subtype="VORBIS", compression_level=1.0 - VORBIS_QUALITY
        )
        decoded, _ = soundfile.read(file)
        measure = Measure.of(decoded)
        if measure.peak_dbfs > PEAK_LIMIT_DBFS:
            raise SystemExit(
                f"{timbre.name}/{cue.file} peaks at {measure.peak_dbfs:.2f} dBFS, above "
                f"{PEAK_LIMIT_DBFS} dBFS; lower {cue.category.name}'s loudness"
            )
        measures[cue.file] = measure
    return measures


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
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--set", choices=sorted(TIMBRES))
    target.add_argument("--audition", type=Path, metavar="DIR")
    parser.add_argument("--out", type=Path, default=REPO / "assets" / "sounds")
    args = parser.parse_args()
    if args.set:
        print_table(args.set, write_set(TIMBRES[args.set], args.out))
        return 0
    sets = {}
    for name, timbre in TIMBRES.items():
        measures = write_set(timbre, args.audition / name)
        print_table(name, measures)
        sets[timbre] = measures
    page = args.audition / "audition.html"
    write_page(page, args.audition, sets)
    print(f"audition page: {page}")
    return 0
