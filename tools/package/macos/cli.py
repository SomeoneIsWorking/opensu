"""Command line: `uv run --frozen python -m tools.package macos [--version V] [--build-dir D]`.

Builds the macOS tree in D (the `tools.check --platform macos` tree, default build/macos-ci) with
the release's version, installs it into openSU.app, carries its dylibs and MoltenVK, signs it
ad hoc and zips it with its .sha256 into D/package/out, then unpacks that and has the app draw a
frame (D/package/smoke/frame.png) and its runner a fake core's (D/package/smoke/runner.png) on the
Mac's Metal device.
"""

import argparse
import subprocess
from pathlib import Path

from tools.check import cmake, deps, macos, process
from tools.package.macos import bundle, config, smoke


def default_version() -> str:
    described = subprocess.run(
        ["git", "-C", str(config.REPO), "describe", "--tags", "--always", "--dirty"],
        capture_output=True, text=True, check=True,
    ).stdout.strip()  # fmt: skip
    return described.removeprefix("v")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="tools.package macos", description=__doc__,
        formatter_class=argparse.RawTextHelpFormatter,
    )  # fmt: skip
    parser.add_argument("--version", default="", help="version in the file name (git describe)")
    parser.add_argument("--build-dir", default=str(config.REPO / "build" / "macos-ci"))
    args = parser.parse_args(argv)
    macos.require_host()
    version = args.version or default_version()
    build_dir = Path(args.build_dir).resolve()
    plan = cmake.Plan(
        build_dir=build_dir,
        deps=deps.Deps(build_dir / "deps"),
        platform="macos",
        build_type="RelWithDebInfo",
        defines=(f"-DOPENSU_VERSION={version}",),
    )
    layout = config.Layout(plan.build_dir)
    env = macos.environment(plan.build_dir, process.build_environment(plan.scratch))
    deps.fetch(plan.deps, env, plan.platform)
    cmake.configure(plan, env)
    cmake.build(plan, env)
    packed = bundle.pack(plan, layout, version, env)
    print(f"app: {packed}", flush=True)
    frame = smoke.run(layout, packed, env)
    print(f"smoke: ok, frame {frame}", flush=True)
    return 0
