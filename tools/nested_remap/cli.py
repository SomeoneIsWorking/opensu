"""Check that a nested Gamescope survives its windows closing and reopening, and ends with a
failure status when its host goes away: `uv run --frozen python -m tools.nested_remap --gamescope
PATH`."""

import argparse
from datetime import datetime
from pathlib import Path

from tools.nested_remap.child import SCENARIOS
from tools.nested_remap.run import RunSpec, run_once

REPO = Path(__file__).resolve().parents[2]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="nested_remap", description=__doc__)
    parser.add_argument("--gamescope", type=Path, required=True, help="the Gamescope binary")
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--cycles", type=int, default=10, help="window close/open cycles per run")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument(
        "--scenario",
        choices=sorted(SCENARIOS),
        default="cycle",
        help="how the windows open and close (tools/nested_remap/child.py)",
    )
    parser.add_argument(
        "--focus-flap",
        action="store_true",
        help="minimize and reactivate Gamescope's host window every 170 ms",
    )
    parser.add_argument(
        "--host-exit-after",
        type=float,
        metavar="SECONDS",
        help="stop the host compositor after SECONDS; the run then passes when "
        "Gamescope exits with a failure status",
    )
    args = parser.parse_args(argv)

    session = REPO / "scratch" / "nested-remap" / datetime.now().strftime("%Y%m%d-%H%M%S")
    failures = 0
    for index in range(args.runs):
        root = session / f"run-{index}"
        spec = RunSpec(args.gamescope.resolve(), args.width, args.height, args.cycles,
                       args.scenario, index, args.focus_flap, args.host_exit_after)  # fmt: skip
        verdict = run_once(spec, root)
        print(f"run {index}: {verdict.describe()}, log {root}", flush=True)
        failures += 0 if verdict.passed else 1
    print(f"{args.runs - failures}/{args.runs} runs passed ({args.scenario}, {args.cycles} cycles)")
    return 1 if failures else 0
