"""The source checks: clang-format, clang-tidy and ruff, each failing on any finding."""

import json
import os
import subprocess
from collections.abc import Mapping
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from tools.check import process
from tools.check.cmake import REPO

CXX_SUFFIXES = (".cpp", ".hpp")
# Only openSU's own sources; Lucent, gpu2d and the fork are checked in their own repositories.
OWN_ROOTS = ("src/", "tests/")


def tracked_cxx(env: Mapping[str, str]) -> list[Path]:
    names = process.output(["git", "-C", REPO, "ls-files", "--", *OWN_ROOTS], env).split()
    return [REPO / name for name in names if name.endswith(CXX_SUFFIXES)]


def clang_format(env: Mapping[str, str]) -> None:
    files = tracked_cxx(env)
    process.run(["clang-format", "--dry-run", "--Werror", *files], env)


def compiled_sources(cmake_dir: Path) -> list[Path]:
    """openSU's translation units in the build's compile_commands.json."""
    entries = json.loads((cmake_dir / "compile_commands.json").read_text())
    own = [str(REPO / root) for root in OWN_ROOTS]
    files = {Path(entry["file"]) for entry in entries}
    return sorted(path for path in files if str(path).startswith(tuple(own)))


def clang_tidy(cmake_dir: Path, env: Mapping[str, str]) -> None:
    """Every translation unit through clang-tidy (`.clang-tidy`: warnings are errors)."""
    files = compiled_sources(cmake_dir)
    if not files:
        raise RuntimeError(f"no openSU sources in {cmake_dir}/compile_commands.json")

    def check(path: Path) -> tuple[Path, int, str]:
        result = subprocess.run(
            ["clang-tidy", "--quiet", "-p", str(cmake_dir), str(path)],
            env=dict(env),
            capture_output=True,
            text=True,
            check=False,
        )
        return path, result.returncode, result.stdout + result.stderr

    print(f"+ clang-tidy over {len(files)} files", flush=True)
    failed = []
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 1) as pool:
        for path, code, text in pool.map(check, files):
            if code != 0:
                failed.append(path)
                print(text, flush=True)
    if failed:
        raise RuntimeError(f"clang-tidy failed on {len(failed)} files")


def ruff(env: Mapping[str, str]) -> None:
    process.run(["ruff", "check", REPO], env)
    process.run(["ruff", "format", "--check", REPO], env)


def all_checks(cmake_dir: Path, env: Mapping[str, str]) -> None:
    clang_format(env)
    ruff(env)
    clang_tidy(cmake_dir, env)
