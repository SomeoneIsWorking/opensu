"""The source checks: clang-format, clang-tidy and ruff, each failing on any finding.

clang-tidy covers the translation units the build compiles, so each platform's tree lints its own
backends and the tests it builds, and none it leaves out.
"""

import json
import re
import subprocess
from collections.abc import Mapping, Sequence
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from tools.check import process
from tools.check.cmake import require_own_tree
from tools.checkout import REPO

CXX_SUFFIXES = (".cpp", ".hpp")
# The analyzer does not step into the MSVC STL, whose bitmask operators cast bit combinations to
# its own enums (`__std_fs_stats_flags`), which optin.core.EnumCastOutOfRange reports.
MSVC_STL_UNINLINED = ("-Xclang", "-analyzer-config", "-Xclang", "c++-stdlib-inlining=false")
# Only openSU's own sources; Lucent, gpu2d and the fork are checked in their own repositories.
OWN_ROOTS = ("src/", "tests/")
# Headers in OWN_ROOTS of this checkout, whatever its directory is called.
_SEPARATOR = r"[/\\]"
HEADER_FILTER = (
    "^"
    + _SEPARATOR.join(map(re.escape, REPO.as_posix().split("/")))
    + f"{_SEPARATOR}({'|'.join(root.rstrip('/') for root in OWN_ROOTS)}){_SEPARATOR}"
)


def tracked(
    env: Mapping[str, str], suffixes: tuple[str, ...], roots: Sequence[str] = ()
) -> list[Path]:
    """The checkout's tracked files under `roots` (all of it when empty) ending in `suffixes`; none
    is an error, since a check over no files passes."""
    listing = process.output(["git", "-C", REPO, "ls-files", "-z", "--", *roots], env)
    files = [REPO / name for name in listing.split("\0") if name.endswith(suffixes)]
    if not files:
        raise RuntimeError(f"git lists no {' '.join(suffixes)} files in {REPO}")
    return files


def clang_format(env: Mapping[str, str]) -> None:
    files = tracked(env, CXX_SUFFIXES, OWN_ROOTS)
    print(f"+ clang-format over {len(files)} files", flush=True)
    process.run(["clang-format", "--dry-run", "--Werror", *files], env)


def built_inputs(cmake_dir: Path, env: Mapping[str, str]) -> set[Path]:
    """Every file the build's default target reads, so targets left out of it are not linted."""
    names = process.output(["ninja", "-C", cmake_dir, "-t", "inputs", "all"], env).splitlines()
    return {(cmake_dir / name).resolve() for name in names}


def compiled_sources(cmake_dir: Path, env: Mapping[str, str]) -> list[Path]:
    """openSU's translation units in the build's compile_commands.json that the build compiles;
    none is an error."""
    require_own_tree(cmake_dir)
    entries = json.loads((cmake_dir / "compile_commands.json").read_text())
    own = [REPO / root for root in OWN_ROOTS]
    built = built_inputs(cmake_dir, env)
    files = {Path(entry["file"]).resolve() for entry in entries}
    sources = sorted(
        path for path in files if any(path.is_relative_to(root) for root in own) and path in built
    )
    if not sources:
        raise RuntimeError(f"{cmake_dir}/compile_commands.json builds no sources of {REPO}")
    return sources


def clang_tidy(
    cmake_dir: Path, env: Mapping[str, str], jobs: int, extra: Sequence[str] = ()
) -> None:
    """Every translation unit through clang-tidy (`.clang-tidy`: warnings are errors) on `jobs`
    processes, each of `extra` put before the compile command's own arguments."""
    files = compiled_sources(cmake_dir, env)

    def check(path: Path) -> tuple[Path, int, str]:
        result = subprocess.run(
            [
                "clang-tidy",
                "--quiet",
                f"--header-filter={HEADER_FILTER}",
                "-p",
                str(cmake_dir),
                *(f"--extra-arg-before={arg}" for arg in extra),
                str(path),
            ],
            env=dict(env),
            capture_output=True,
            text=True,
            check=False,
        )
        return path, result.returncode, result.stdout + result.stderr

    print(f"+ clang-tidy over {len(files)} files on {jobs} processes", flush=True)
    failed = []
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        for path, code, text in pool.map(check, files):
            if code != 0:
                failed.append(path)
                print(text, flush=True)
    if failed:
        raise RuntimeError(f"clang-tidy failed on {len(failed)} files")


def ruff(env: Mapping[str, str]) -> None:
    files = tracked(env, (".py",))
    print(f"+ ruff over {len(files)} files", flush=True)
    process.run(["ruff", "check", *files], env)
    process.run(["ruff", "format", "--check", *files], env)


def all_checks(cmake_dir: Path, env: Mapping[str, str], jobs: int, platform: str = "linux") -> None:
    """Every check, on the Linux tree. A macOS or Windows tree runs clang-tidy alone, over its own
    translation units: the format and the Python are the same on every OS."""
    if platform == "windows":
        clang_tidy(cmake_dir, env, jobs, MSVC_STL_UNINLINED)
        return
    if platform == "macos":
        clang_tidy(cmake_dir, env, jobs)
        return
    clang_format(env)
    ruff(env)
    clang_tidy(cmake_dir, env, jobs)
