"""Command line: `uv run --frozen python -m tools.package windows [--version V] [--build-dir D]`.

Builds the Windows tree in D (the `tools.check --platform windows` tree, default build/windows-ci)
with the release's version, installs it into a stage, packs the per-user Inno Setup installer
with its .sha256 into D/package/out, then installs that silently into a scratch folder and has
the installed opensu.exe draw a frame (D/package/smoke/frame.png) through lavapipe.
"""

import argparse
import subprocess

from tools.check import cmake, deps, process, windows
from tools.package.windows import config, installer, smoke


def default_version() -> str:
    described = subprocess.run(
        ["git", "-C", str(config.REPO), "describe", "--tags", "--always", "--dirty"],
        capture_output=True, text=True, check=True,
    ).stdout.strip()  # fmt: skip
    return described.removeprefix("v")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="tools.package windows", description=__doc__,
        formatter_class=argparse.RawTextHelpFormatter,
    )  # fmt: skip
    parser.add_argument("--version", default="", help="version in the file name (git describe)")
    parser.add_argument("--build-dir", default=str(config.REPO / "build" / "windows-ci"))
    args = parser.parse_args(argv)
    windows.require_host()
    version = args.version or default_version()
    plan = cmake.Plan(
        build_dir=cmake.Path(args.build_dir).resolve(),
        deps=deps.Deps(cmake.Path(args.build_dir).resolve() / "deps"),
        platform="windows",
        build_type="RelWithDebInfo",
        defines=(f"-DOPENSU_VERSION={version}",),
    )
    layout = config.Layout(plan.build_dir)
    env = windows.environment(plan.build_dir, process.build_environment(plan.scratch))
    deps.fetch(plan.deps, env, plan.platform)
    cmake.configure(plan, env)
    cmake.build(plan, env)
    packed = installer.pack(plan, layout, version, env)
    print(f"installer: {packed}", flush=True)
    frame = smoke.run(layout, packed, windows.lavapipe(plan.build_dir / "lavapipe", env))
    print(f"smoke: ok, frame {frame}", flush=True)
    return 0
