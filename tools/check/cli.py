"""Command line: `uv run --frozen python -m tools.check <step> [--platform P] [--build-dir D]
[--gamescope M]`.

Steps run in order: deps (pinned Lucent and gpu2d, and vcpkg for Windows), configure, build, lint,
test; `all` runs every one. `ci` runs `all --gamescope native` inside the CI image (packaging/ci,
Fedora 44 with systemd), as the CI workflow does; a maintainer can run any step against the same
tree. `--platform windows` builds on a Windows host with clang-cl and vcpkg
(tools/check/windows.py).
"""

import argparse
from collections.abc import Callable, Mapping
from pathlib import Path

from tools.check import cmake, container, deps, lint, process, windows

STEPS = ("deps", "configure", "build", "lint", "test")


def plan_from(args: argparse.Namespace) -> cmake.Plan:
    build_dir = Path(args.build_dir).resolve()
    return cmake.Plan(
        build_dir=build_dir,
        deps=deps.Deps(build_dir / "deps"),
        platform=args.platform,
        gamescope=args.gamescope,
    )


def step_actions(plan: cmake.Plan) -> dict[str, Callable[[Mapping[str, str]], None]]:
    return {
        "deps": lambda env: deps.fetch(plan.deps, env, plan.platform),
        "configure": lambda env: cmake.configure(plan, env),
        "build": lambda env: cmake.build(plan, env),
        "lint": lambda env: lint.all_checks(plan.cmake_dir, env, plan.platform),
        "test": lambda env: cmake.test(plan, env),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="tools.check", description=__doc__.splitlines()[0])
    parser.add_argument("step", choices=(*STEPS, "all", "ci"))
    parser.add_argument("--platform", choices=cmake.PLATFORMS, default="linux")
    parser.add_argument("--build-dir", default=str(cmake.REPO / "build" / "check"))
    parser.add_argument(
        "--gamescope",
        choices=cmake.GAMESCOPE_MODES,
        default="podman",
        help="how the Gamescope fork is built (default podman)",
    )
    args = parser.parse_args(argv)
    plan = plan_from(args)
    env = process.build_environment(plan.scratch)
    if plan.platform == "windows":
        windows.require_host()
        env = windows.environment(plan.build_dir, env)
    if args.step == "ci":
        container.run_all(plan.build_dir, env)
        return 0
    actions = step_actions(plan)
    for step in STEPS if args.step == "all" else (args.step,):
        print(f"== {step}", flush=True)
        actions[step](env)
    return 0
