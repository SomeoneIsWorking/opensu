"""Command line: `uv run --frozen python -m tools.check <step> [--platform P] [--build-dir D]
[--gamescope M] [--jobs N]`, from the checkout to check (`--directory` for another one).

Steps run in order: deps (pinned Lucent and gpu2d, and vcpkg for Windows), configure, build, lint,
test; `all` runs every one. On Linux every step but test runs in the build image (packaging/ci on
the host's Fedora release, tools/check/container.py) and test runs on the host. `ci` runs `all`
inside the build image on the pinned Fedora 44 with systemd, as the CI workflow does.
`--platform windows` builds on a Windows host with clang-cl and vcpkg (tools/check/windows.py),
`--platform macos` on an Apple silicon Mac with Homebrew's LLVM and vcpkg (tools/check/macos.py).
`--jobs` caps the compilers and clang-tidy processes that run at once.
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
        "deps": lambda env: cmake.prepare(plan, env),
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
        default="native",
        help="native builds the Gamescope fork in the build image (default), off leaves it out",
    )
    parser.add_argument(
        "--in-build-image",
        action="store_true",
        help="run every step in place: the build image's own run, which tools.check starts",
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
    if args.in_build_image:
        container.require_inside()
    if args.step == "ci":
        container.require_podman(env)
        container.run_all(plan.build_dir, plan.jobs, env)
        return 0
    in_image = plan.platform == "linux" and not args.in_build_image
    if in_image:
        container.require_podman(env)
    actions = step_actions(plan)
    for step in STEPS if args.step == "all" else (args.step,):
        print(f"== {step}", flush=True)
        if in_image and step in container.IN_IMAGE:
            container.run_step(step, plan.build_dir, plan.gamescope, plan.jobs, env)
        else:
            actions[step](env)
    return 0
