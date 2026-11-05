"""Command line: `uv run --frozen python -m tools.check <step> [--platform P] [--build-dir D]
[--gamescope M] [--jobs N]`, from the checkout to check (`--directory` for another one).
`lint --changed [--base REF]` lints what changed against REF's merge base, uncommitted files
included, and clang-tidy the translation units that reach it; plain `lint` is the full check.

Steps run in order: deps (pinned Lucent and gpu2d, and vcpkg for Windows), configure, build, test,
lint, so a lint finding never hides what the tests say; `all` runs every one. On Linux every step
but test runs in the build image (packaging/ci on the host's Fedora release,
tools/check/container.py) and test runs on the host. `ci` runs `all` inside the build image on the pinned Fedora 44 with systemd, as the CI workflow does. `install`
(not part of `all`) installs the tree into `--prefix` (default ~/.local) and, on Linux, puts the
menu entry it installed on the desktop too (tools/check/shortcut.py); `--destdir` stages it instead.
`--platform windows` builds on a Windows host with clang-cl and vcpkg (tools/check/windows.py),
`--platform macos` on an Apple silicon Mac with Homebrew's LLVM and vcpkg (tools/check/macos.py).
`--jobs` caps the compilers and clang-tidy processes that run at once.
"""

import argparse
from collections.abc import Callable, Mapping
from pathlib import Path

from tools.check import cmake, container, deps, lint, macos, process, shortcut, windows
from tools.checkout import REPO, require_own_environment

STEPS = ("deps", "configure", "build", "test", "lint")
# Every component: the shell, the runner, the fork, assets, the menu entry and the icon.
INSTALL_COMPONENT = "Unspecified"


def plan_from(args: argparse.Namespace) -> cmake.Plan:
    build_dir = Path(args.build_dir).resolve()
    return cmake.Plan(
        build_dir=build_dir,
        deps=deps.Deps(build_dir / "deps"),
        jobs=args.jobs,
        platform=args.platform,
        gamescope=args.gamescope,
    )


def install(plan: cmake.Plan, prefix: Path, destdir: Path | None, env: Mapping[str, str]) -> None:
    """Everything the tree installs into `prefix`, under `destdir` when one is given."""
    staged = {**env, "DESTDIR": str(destdir)} if destdir is not None else env
    cmake.install_to(plan, prefix, INSTALL_COMPONENT, staged)


def lint_step(plan: cmake.Plan, base: str | None, env: Mapping[str, str]) -> None:
    """The full lint, or the changed files' against `base` when one is given."""
    if base is None:
        lint.all_checks(plan.cmake_dir, env, plan.jobs, plan.platform)
    else:
        lint.changed_checks(plan.cmake_dir, env, plan.jobs, plan.platform, base)


def step_actions(
    plan: cmake.Plan, prefix: Path, destdir: Path | None, base: str | None
) -> dict[str, Callable[[Mapping[str, str]], None]]:
    return {
        "install": lambda env: install(plan, prefix, destdir, env),
        "deps": lambda env: cmake.prepare(plan, env),
        "configure": lambda env: cmake.configure(plan, env),
        "build": lambda env: cmake.build(plan, env),
        "lint": lambda env: lint_step(plan, base, env),
        "test": lambda env: cmake.test(plan, env),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="tools.check", description=__doc__.splitlines()[0])
    parser.add_argument("step", choices=(*STEPS, "all", "ci", "install"))
    parser.add_argument("--platform", choices=cmake.PLATFORMS, default="linux")
    parser.add_argument("--build-dir", default=str(REPO / "build" / "check"))
    parser.add_argument(
        "--gamescope",
        choices=cmake.GAMESCOPE_MODES,
        default="on",
        help="on stages the shared prefix's Gamescope fork in the tree (default), off omits it",
    )
    parser.add_argument(
        "--prefix",
        default=str(Path.home() / ".local"),
        help="where install puts the tree (default ~/.local)",
    )
    parser.add_argument(
        "--destdir",
        help="install under this root, as a package stages it; no desktop shortcut is written",
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
    parser.add_argument(
        "--changed",
        action="store_true",
        help="lint only: check what changed against --base, not every file",
    )
    parser.add_argument(
        "--base", help=f"the ref --changed compares with (default {lint.DEFAULT_BASE})"
    )
    args = parser.parse_args(argv)
    if args.jobs < 1:
        parser.error("--jobs must be at least 1")
    if args.changed and args.step != "lint":
        parser.error("--changed goes with the lint step")
    if args.base is not None and not args.changed:
        parser.error("--base goes with --changed")
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
    prefix = Path(args.prefix).resolve()
    destdir = Path(args.destdir).resolve() if args.destdir else None
    base = (args.base or lint.DEFAULT_BASE) if args.changed else None
    actions = step_actions(plan, prefix, destdir, base)
    options = ["--gamescope", plan.gamescope, "--jobs", str(plan.jobs), "--prefix", str(prefix)]
    if destdir is not None:
        options += ["--destdir", str(destdir)]
    if base is not None:
        options += ["--changed", "--base", base]
    writes = [destdir or prefix] if args.step == "install" else []
    for step in STEPS if args.step == "all" else (args.step,):
        print(f"== {step}", flush=True)
        if in_image and step in container.IN_IMAGE:
            container.run_step(step, plan.build_dir, options, writes, env)
        else:
            actions[step](env)
    placed_here = destdir is None and not args.in_build_image and plan.platform == "linux"
    if args.step == "install" and placed_here:
        shortcut.place(prefix, env)
    return 0
