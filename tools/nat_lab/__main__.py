"""Runs a case of two hidden openSUs across simulated NATs, re-entering a user namespace first."""

import argparse
import os
import sys
from pathlib import Path

from tools.checkout import REPO

INSIDE = "NAT_LAB_INSIDE"


def main() -> int:
    from tools.nat_lab import cases

    parser = argparse.ArgumentParser(prog="tools.nat_lab", description=__doc__)
    parser.add_argument("case", choices=[*cases.CASES, "all"])
    parser.add_argument("--out", type=Path, default=REPO / "scratch" / "wan-netplay-proof")
    parser.add_argument("--build", type=Path, default=REPO / "build" / "check" / "cmake")
    args = parser.parse_args()
    if INSIDE not in os.environ:
        os.environ[INSIDE] = "1"
        command = [sys.executable, "-m", "tools.nat_lab", *sys.argv[1:]]
        unshare = ["unshare", "--user", "--map-current-user", "--keep-caps", "--", *command]
        os.execvp("unshare", unshare)
    names = list(cases.CASES) if args.case == "all" else [args.case]
    for name in names:
        print(f"== {name}", flush=True)
        cases.CASES[name](cases.Context(args.out / name, args.build, REPO))
        print(f"== {name}: passed", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
