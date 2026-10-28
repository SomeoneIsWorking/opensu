"""Command line: `uv run --frozen python -m tools.appimage <command>`.

  build [--version V]   build the AppImage in the Ubuntu 24.04 build container
  smoke [--version V]   run the built AppImage's checks on every test distribution
  all   [--version V]   build, then smoke

The AppImage, its .sha256 and .zsync land in build/appimage/out. With OPENSU_APPIMAGE_SIGNING_KEY
set to an armored private key, the AppImage is signed with it.
"""

import argparse
import subprocess

from tools.appimage import config, container, inner, smoke
from tools.check import process


def default_version() -> str:
    described = subprocess.run(
        ["git", "-C", str(config.REPO), "describe", "--tags", "--always", "--dirty"],
        capture_output=True, text=True, check=True,
    ).stdout.strip()  # fmt: skip
    return described.removeprefix("v")


def build(layout: config.Layout, version: str) -> None:
    env = process.build_environment(layout.scratch)
    tag = container.ensure_image(env)
    container.run_in(
        tag,
        ["uv", "run", "--frozen", "python", "-m", "tools.appimage", "inner", "--version", version],
        layout,
        env,
        passthrough=(config.SIGNING_KEY_ENV,),
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="tools.appimage", description=__doc__, formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument("command", choices=("build", "smoke", "all", "inner"))
    parser.add_argument("--version", default="", help="version in the file name (git describe)")
    parser.add_argument(
        "--distro", action="append", choices=sorted(smoke.DISTROS), help="smoke only these"
    )
    args = parser.parse_args(argv)
    layout = config.default_layout()
    version = args.version or default_version()
    if args.command == "inner":
        inner.build(layout, version)
        return 0
    if args.command in ("build", "all"):
        build(layout, version)
    if args.command in ("smoke", "all"):
        smoke.run_all(layout, version, args.distro or sorted(smoke.DISTROS))
    return 0
