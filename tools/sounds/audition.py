"""One self-contained HTML page to compare the candidate sets on: every cue embedded as a data URI,
a play button per cue, grouped by set, with its measures."""

import base64
import html
from pathlib import Path

from tools.sounds.loudness import Measure
from tools.sounds.timbres import Timbre

EVENTS = {
    "navigation.ogg": "Focus moves in a list or the grid",
    "open.ogg": "A panel appears",
    "close.ogg": "A panel or page is dismissed",
    "context-menu.ogg": "The context menu or a picker opens",
    "launch.ogg": "A game launches",
    "enter.ogg": "A opens a console, launcher or collection",
    "exit.ogg": "B backs out of one",
    "section-1.ogg": "L1/R1 to a section of 1 tile",
    "section-2.ogg": "L1/R1 to a section of 2 tiles",
    "section-3-5.ogg": "L1/R1 to a section of 3-5 tiles",
    "section-6-11.ogg": "L1/R1 to a section of 6-11 tiles",
    "section-12-plus.ogg": "L1/R1 to a section of 12+ tiles",
}

STYLE = """
:root { --bg: #f6f5f2; --card: #ffffff; --ink: #1d1d1f; --muted: #6b6b70; --line: #e3e1dc;
  --accent: #2f6fde; --accent-ink: #ffffff; }
@media (prefers-color-scheme: dark) { :root { --bg: #151517; --card: #1f1f22; --ink: #ececee;
  --muted: #9a9aa2; --line: #2e2e33; --accent: #6d9bff; --accent-ink: #0d0d10; } }
* { box-sizing: border-box; }
body { margin: 0; background: var(--bg); color: var(--ink);
  font: 15px/1.45 system-ui, -apple-system, "Segoe UI", sans-serif; }
main { max-width: 1040px; margin: 0 auto; padding: 24px 16px 48px; }
h1 { font-size: 22px; margin: 0 0 4px; }
p.lead { color: var(--muted); margin: 0 0 20px; }
section { background: var(--card); border: 1px solid var(--line); border-radius: 12px;
  padding: 16px; margin-bottom: 16px; }
h2 { font-size: 18px; margin: 0 0 2px; text-transform: capitalize; }
section > p { color: var(--muted); margin: 0 0 12px; }
.row-actions { margin-bottom: 10px; }
.scroll { overflow-x: auto; }
table { border-collapse: collapse; width: 100%; font-variant-numeric: tabular-nums; }
th, td { text-align: left; padding: 6px 8px; border-top: 1px solid var(--line);
  white-space: nowrap; }
th { color: var(--muted); font-weight: 500; font-size: 13px; border-top: none; }
td.num, th.num { text-align: right; }
td.event { white-space: normal; min-width: 180px; }
code { font-size: 13px; }
button { font: inherit; border: none; border-radius: 999px; padding: 4px 14px; cursor: pointer;
  background: var(--accent); color: var(--accent-ink); }
button.ghost { background: transparent; color: var(--accent); border: 1px solid var(--accent); }
"""

SCRIPT = """
function play(id) { const a = document.getElementById(id); a.currentTime = 0; a.play(); }
async function playAll(set) {
  for (const a of document.querySelectorAll('audio[data-set="' + set + '"]')) {
    a.currentTime = 0; await a.play();
    await new Promise(r => setTimeout(r, a.duration * 1000 + 350));
  }
}
"""


def _row(set_name: str, file: str, measure: Measure, data: bytes) -> str:
    ident = f"{set_name}-{file}".replace(".", "-")
    source = "data:audio/ogg;base64," + base64.b64encode(data).decode("ascii")
    return (
        f"<tr><td><button onclick=\"play('{ident}')\">Play</button>"
        f'<audio id="{ident}" data-set="{set_name}" preload="auto" src="{source}"></audio></td>'
        f"<td><code>{html.escape(file)}</code></td>"
        f'<td class="event">{html.escape(EVENTS[file])}</td>'
        f'<td class="num">{measure.seconds * 1000:.0f}</td>'
        f'<td class="num">{measure.peak_dbfs:.1f}</td>'
        f'<td class="num">{measure.lufs:.1f}</td>'
        f'<td class="num">{measure.rms_dbfs:.1f}</td>'
        f'<td class="num">{measure.high_share * 100:.2f}</td></tr>'
    )


def write_page(page: Path, root: Path, sets: dict[Timbre, dict[str, Measure]]) -> None:
    """Writes `page` embedding each set's files from `root`/<set>/."""
    sections = []
    for timbre, measures in sets.items():
        rows = "".join(
            _row(timbre.name, file, measure, (root / timbre.name / file).read_bytes())
            for file, measure in measures.items()
        )
        sections.append(
            f"<section><h2>{html.escape(timbre.name)}</h2><p>{html.escape(timbre.summary)}</p>"
            f'<div class="row-actions"><button class="ghost" '
            f"onclick=\"playAll('{timbre.name}')\">Play all in order</button></div>"
            '<div class="scroll"><table><tr><th></th><th>File</th><th>Event</th>'
            '<th class="num">ms</th><th class="num">Peak dBFS</th><th class="num">LUFS</th>'
            f'<th class="num">RMS dBFS</th><th class="num">&gt;6 kHz %</th></tr>{rows}'
            "</table></div></section>"
        )
    page.write_text(
        '<!doctype html><html lang="en"><head><meta charset="utf-8">'
        '<meta name="viewport" content="width=device-width, initial-scale=1">'
        f"<title>openSU Sound Audition</title><style>{STYLE}</style></head><body><main>"
        "<h1>openSU sound audition</h1>"
        '<p class="lead">Every set plays the same pentatonic score. LUFS is the loudest 400 ms '
        "window (BS.1770 momentary), matched within each category.</p>"
        f"{''.join(sections)}</main><script>{SCRIPT}</script></body></html>",
        encoding="utf-8",
    )
