"""Command line: `uv run --frozen python -m tools.check <step> [--platform P] [--build-dir D]
[--gamescope M] [--jobs N]`, from the checkout to check (`--directory` for another one).

Steps run in order: deps (pinned Lucent and gpu2d, and vcpkg for Windows), configure, build, lint,
test; `all` runs every one. `ci` runs `all --gamescope native` inside the CI image (packaging/ci,
Fedora 44 with systemd), as the CI workflow does; a maintainer can run any step against the same
tree. `--platform windows` builds on a Windows host with clang-cl and vcpkg
(tools/check/windows.py), `--platform macos` on an Apple silicon Mac with Homebrew's LLVM and vcpkg
(tools/check/macos.py). `--jobs` caps the compilers and clang-tidy processes that run at once.
"""

import argparse
from collections.abc import Callable, Mapping
from pathlib import Path

from tools.check import cmake, container, deps, lint, macos, process, windows
from tools.checkout import REPO, require_own_environment

STEPS = ("deps", "configure", "build", "lint", "test")


def plan_from(args: argparse.Namespace) -> cmake.Plan:
    build_dir = Path(args.build_dir).resolve()
    return cmake.Plan(
        build_dir=build_dir,
        deps=deps.Deps(build_dir / "deps"),
        jobs=args.jobs,
        platform=args.platform,
        gamescope=args.gamescope,
    )


def step_actions(plan: cmake.Plan) -> dict[str, Callable[[Mapping[str, str]], None]]:
    return {
        "deps": lambda env: deps.fetch(plan.deps, env, plan.platform),
        "configure": lambda env: cmake.configure(plan, env),
        "build": lambda env: cmake.build(plan, env),
        "lint": lambda env: lint.all_checks(plan.cmake_dir, env, plan.jobs, plan.platform),
        "test": lambda env: cmake.test(plan, env),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="tools.check", description=__doc__.splitlines()[0])
    parser.add_argument("step", choices=(*STEPS, "all", "ci"))
    parser.add_argument("--platform", choices=cmake.PLATFORMS, default="linux")
    parser.add_argument("--build-dir", default=str(REPO / "build" / "check"))
    parser.add_argument(
        "--gamescope",
        choices=cmake.GAMESCOPE_MODES,
        default="podman",
        help="how the Gamescope fork is built (default podman)",
    )
    parser.add_argument(
        "--jobs",
        type=int,
        default=cmake.DEFAULT_JOBS,
        help="compilers and clang-tidy processes run at once (default: one per core)",
    )
    args = parser.parse_args(argv)
    if args.jobs < 1:
        parser.error("--jobs must be at least 1")
    require_own_environment()
    plan = plan_from(args)
    cmake.require_own_tree(plan.cmake_dir)
    env = process.build_environment(plan.scratch)
    if plan.platform == "windows":
        windows.require_host()
        env = windows.environment(plan.build_dir, env)
    if plan.platform == "macos":
        macos.require_host()
        env = macos.environment(plan.build_dir, env)
    if args.step == "ci":
        container.run_all(plan.build_dir, plan.jobs, env)
        return 0
    actions = step_actions(plan)
    for step in STEPS if args.step == "all" else (args.step,):
        print(f"== {step}", flush=True)
        actions[step](env)
    return 0
