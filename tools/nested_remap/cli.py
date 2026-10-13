"""Check that a nested Gamescope survives its windows closing and reopening:
`uv run --frozen python -m tools.nested_remap --gamescope PATH`."""

import argparse
from datetime import datetime
from pathlib import Path

from tools.nested_remap.run import RunSpec, run_once

REPO = Path(__file__).resolve().parents[2]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="nested_remap", description=__doc__)
    parser.add_argument("--gamescope", type=Path, required=True, help="the Gamescope binary")
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--cycles", type=int, default=10, help="window close/open cycles per run")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    args = parser.parse_args(argv)

    spec = RunSpec(args.gamescope.resolve(), args.width, args.height, args.cycles)
    session = REPO / "scratch" / "nested-remap" / datetime.now().strftime("%Y%m%d-%H%M%S")
    failures = 0
    for index in range(args.runs):
        root = session / f"run-{index}"
        verdict = run_once(spec, root)
        state = "survived" if verdict.survived else "died"
        detail = f" ({verdict.protocol_error})" if verdict.protocol_error else ""
        print(f"run {index}: exit {verdict.exit_code}, {state}{detail}, log {root}", flush=True)
        failures += 0 if verdict.survived else 1
    print(f"{args.runs - failures}/{args.runs} runs survived {args.cycles} cycles each")
    return 1 if failures else 0
