"""The cut: a title card, each shot under its caption, an end card, crossfades between them and
the trailer's own music under it all."""

import subprocess
from dataclasses import dataclass
from pathlib import Path

from tools.checkout import REPO

FONT = REPO / "assets" / "CalSans-Regular.ttf"
GROUND = "0x111317"
ACCENT = "0x34B4E4"
CROSSFADE = 0.5
FADE_OUT = 2.5
# YouTube plays everything at about -14 LUFS; louder only gets turned down.
LOUDNESS = -14


@dataclass(frozen=True)
class Format:
    width: int
    height: int
    fps: int


@dataclass(frozen=True)
class Line:
    text: str
    size: int
    colour: str = "white"


def seconds(clip: Path) -> float:
    probe = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", str(clip)],
        check=True,
        capture_output=True,
        text=True,
    )
    return float(probe.stdout.strip())


def _text(work: Path, name: str, text: str) -> Path:
    """Text for drawtext as a file, so nothing in it needs escaping."""
    path = work / f"{name}.txt"
    path.write_text(text)
    return path


def _ffmpeg(*words: str) -> None:
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", *words], check=True)


def _encode_args(out: Path) -> list[str]:
    return ["-c:v", "libx264", "-preset", "slow", "-crf", "14", "-pix_fmt", "yuv420p", str(out)]


def card(lines: list[Line], length: float, fmt: Format, work: Path, out: Path) -> Path:
    """Centred lines on the dark ground, with a thin accent rule under the first."""
    scale = fmt.height / 1080
    gap = round(28 * scale)
    heights = [round(line.size * scale) for line in lines]
    total = sum(heights) + gap * (len(lines) - 1)
    top = (fmt.height - total) // 2
    filters = []
    y = top
    for index, (line, height) in enumerate(zip(lines, heights, strict=True)):
        text = _text(work, f"{out.stem}-{index}", line.text)
        fade = f"if(lt(t,{0.25 + index * 0.35}),0,min(1,(t-{0.25 + index * 0.35})/0.6))"
        filters.append(
            f"drawtext=fontfile={FONT}:textfile={text}:fontsize={height}:fontcolor={line.colour}"
            f":x=(w-text_w)/2:y={y}:alpha='{fade}'"
        )
        if index == 0:
            rule_y = y + height + gap // 2
            filters.append(
                f"drawbox=x=(iw-{round(160 * scale)})/2:y={rule_y}:w={round(160 * scale)}"
                f":h={max(2, round(5 * scale))}:color={ACCENT}:t=fill"
                f":enable='gte(t,0.5)'"
            )
            y += gap
        y += height + gap
    _ffmpeg(
        "-f",
        "lavfi",
        "-i",
        f"color=c={GROUND}:s={fmt.width}x{fmt.height}:r={fmt.fps}:d={length}",
        "-vf",
        ",".join(filters),
        *_encode_args(out),
    )
    return out


def list_card(
    title: str, columns: list[list[str]], length: float, fmt: Format, work: Path, out: Path
) -> Path:
    """`title` over columns of short lines, each line fading in after the one before it, a dot in
    the accent colour before each."""
    scale = fmt.height / 1080
    size = round(40 * scale)
    title_size = round(76 * scale)
    gap = round(30 * scale)
    rows = max(len(column) for column in columns)
    total = title_size + gap * 2 + rows * size + (rows - 1) * gap
    top = (fmt.height - total) // 2
    title_file = _text(work, f"{out.stem}-title", title)
    filters = [
        f"drawtext=fontfile={FONT}:textfile={title_file}:fontsize={title_size}:fontcolor=white"
        f":x=(w-text_w)/2:y={top}:alpha='min(1,t/0.5)'"
    ]
    first = top + title_size + gap * 2
    width = fmt.width // len(columns)
    left = round(130 * scale)
    shown = 0
    for row in range(rows):
        for index, column in enumerate(columns):
            if row >= len(column):
                continue
            start = 0.5 + shown * 0.25
            shown += 1
            fade = f"if(lt(t,{start:.2f}),0,min(1,(t-{start:.2f})/0.4))"
            x = index * width + left
            y = first + row * (size + gap)
            dot = round(12 * scale)
            filters.append(
                f"drawbox=x={x}:y={y + (size - dot) // 2}:w={dot}:h={dot}:color={ACCENT}:t=fill"
                f":enable='gte(t,{start:.2f})'"
            )
            text = _text(work, f"{out.stem}-{index}-{row}", column[row])
            filters.append(
                f"drawtext=fontfile={FONT}:textfile={text}:fontsize={size}:fontcolor=0xE6E8EB"
                f":x={x + dot * 3}:y={y}:alpha='{fade}'"
            )
    _ffmpeg(
        "-f",
        "lavfi",
        "-i",
        f"color=c={GROUND}:s={fmt.width}x{fmt.height}:r={fmt.fps}:d={length}",
        "-vf",
        ",".join(filters),
        *_encode_args(out),
    )
    return out


def captioned(clip: Path, caption: str, longest: float, fmt: Format, work: Path, out: Path) -> Path:
    """The clip, sped up to `longest` seconds if it runs over, with its caption low and centred."""
    length = seconds(clip)
    speed = f"setpts=PTS*{longest / length:.6f}," if length > longest else ""
    scale = fmt.height / 1080
    text = _text(work, f"{out.stem}-caption", caption)
    size = round(50 * scale)
    caption_filter = (
        f"drawtext=fontfile={FONT}:textfile={text}:fontsize={size}:fontcolor=white"
        f":box=1:boxcolor=black@0.6:boxborderw={round(26 * scale)}"
        f":x=(w-text_w)/2:y=h-text_h-{round(96 * scale)}"
        f":alpha='if(lt(t,0.3),0,min(1,(t-0.3)/0.4))'"
    )
    _ffmpeg(
        "-i",
        str(clip),
        "-vf",
        f"{speed}fps={fmt.fps},{caption_filter}",
        *_encode_args(out),
    )
    return out


def total_length(parts: list[Path]) -> float:
    """How long the parts run crossfaded into one another."""
    return sum(seconds(part) for part in parts) - CROSSFADE * (len(parts) - 1)


def assemble(parts: list[Path], track: Path, fmt: Format, out: Path) -> Path:
    """The parts crossfaded in order, with `track` under them, faded at both ends."""
    lengths = [seconds(part) for part in parts]
    inputs: list[str] = []
    for part in parts:
        inputs += ["-i", str(part)]
    chain = []
    label = "[0:v]"
    offset = 0.0
    for index in range(1, len(parts)):
        offset += lengths[index - 1] - CROSSFADE
        joined = f"[x{index}]"
        chain.append(
            f"{label}[{index}:v]xfade=transition=fade:duration={CROSSFADE}:offset={offset:.3f}{joined}"
        )
        label = joined
    total = sum(lengths) - CROSSFADE * (len(parts) - 1)
    music = len(parts)
    chain.append(f"{label}format=yuv420p[v]")
    chain.append(
        f"[{music}:a]atrim=0:{total:.3f},afade=t=in:d=1.0,"
        f"afade=t=out:st={total - FADE_OUT:.3f}:d={FADE_OUT},"
        f"loudnorm=I={LOUDNESS}:TP=-1.5:LRA=11[a]"
    )
    _ffmpeg(
        *inputs,
        "-i",
        str(track),
        "-filter_complex",
        ";".join(chain),
        "-map",
        "[v]",
        "-map",
        "[a]",
        "-r",
        str(fmt.fps),
        "-c:v",
        "libx264",
        "-preset",
        "slow",
        "-crf",
        "16",
        "-pix_fmt",
        "yuv420p",
        "-c:a",
        "aac",
        "-b:a",
        "192k",
        "-movflags",
        "+faststart",
        "-t",
        f"{total:.3f}",
        str(out),
    )
    return out
