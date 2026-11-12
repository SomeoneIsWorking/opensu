"""Runs a case of two hidden openSUs across simulated NATs, re-entering a user namespace first, or
the Android case: an AVD's openSU meets a hidden one on this machine's own network."""

import argparse
import os
import sys
from pathlib import Path

from tools.checkout import REPO

INSIDE = "NAT_LAB_INSIDE"
# Runs on this machine's own network, apart from "all".
ANDROID = "android"


def main() -> int:
    from tools.nat_lab import cases

    parser = argparse.ArgumentParser(prog="tools.nat_lab", description=__doc__)
    parser.add_argument("case", choices=[*cases.CASES, "all", ANDROID])
    parser.add_argument("--out", type=Path, default=REPO / "scratch" / "wan-netplay-proof")
    parser.add_argument("--build", type=Path, default=REPO / "build" / "check" / "cmake")
    parser.add_argument("--avd", help=f"{ANDROID} only: the AVD to boot, from ANDROID_AVD_HOME")
    parser.add_argument("--apk", type=Path, help=f"{ANDROID} only: an APK to install first")
    args = parser.parse_args()
    if args.case == ANDROID:
        from tools.nat_lab import android_case

        if not args.avd:
            parser.error(f"{ANDROID} needs --avd")
        print(f"== {ANDROID}", flush=True)
        android_case.run(
            android_case.Setup(args.out / ANDROID, args.build, REPO, args.avd, args.apk)
        )
        print(f"== {ANDROID}: passed", flush=True)
        return 0
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
