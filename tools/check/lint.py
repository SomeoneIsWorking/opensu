"""The source checks: clang-format, clang-tidy and ruff, each failing on any finding.

clang-tidy covers the translation units the build compiles, so each platform's tree lints its own
backends and the tests it builds, and none it leaves out. `changed_checks` runs the same checks over
what a change can reach; `all_checks` is the full check, which CI runs.
"""

import json
import re
import subprocess
from collections.abc import Iterable, Mapping, Sequence
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path

from tools.check import process
from tools.check.cmake import require_own_tree
from tools.checkout import REPO

CXX_SUFFIXES = (".cpp", ".hpp")
DEFAULT_BASE = "origin/main"
# A change to any of these reaches every file, whatever the file's own diff says.
BUILD_SETTING_NAMES = (".clang-format", ".clang-tidy", "CMakeLists.txt")
BUILD_SETTING_SUFFIX = ".cmake"
# A STALE record is older than its object, so it does not say what the object read.
DEPS_RECORD = re.compile(r"(.+): #deps \d+, deps mtime \d+ \(VALID\)")
DEPS_LINE_PREFIX = "    "
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


@dataclass(frozen=True)
class Scope:
    """The files one lint run covers: what clang-format, ruff and clang-tidy each get."""

    cxx: Sequence[Path]
    python: Sequence[Path]
    translation_units: Sequence[Path]


def within(
    files: Iterable[Path], suffixes: tuple[str, ...], roots: Sequence[str] = ()
) -> list[Path]:
    """Those of `files` that exist, end in `suffixes` and lie under `roots` (any when empty)."""
    under = [REPO / root for root in roots] or [REPO]
    return sorted(
        path
        for path in files
        if path.name.endswith(suffixes)
        and any(path.is_relative_to(root) for root in under)
        and path.is_file()
    )


def tracked(
    env: Mapping[str, str], suffixes: tuple[str, ...], roots: Sequence[str] = ()
) -> list[Path]:
    """The checkout's tracked files under `roots` (all of it when empty) ending in `suffixes`; none
    is an error, since a check over no files passes."""
    listing = process.output(["git", "-C", REPO, "ls-files", "-z"], env)
    files = within((REPO / name for name in listing.split("\0") if name), suffixes, roots)
    if not files:
        raise RuntimeError(f"git lists no {' '.join(suffixes)} files in {REPO}")
    return files


def changed_files(base: str, env: Mapping[str, str]) -> set[Path]:
    """What differs from the merge base with `base`, committed or not, and the untracked files; a
    deleted or renamed-away file is listed too, since what included it is affected."""
    fork = process.output(["git", "-C", REPO, "merge-base", base, "HEAD"], env).strip()
    diff = ["git", "-C", REPO, "diff", "--name-only", "--no-renames", "-z", fork, "--"]
    untracked = ["git", "-C", REPO, "ls-files", "--others", "--exclude-standard", "-z"]
    names = process.output(diff, env) + process.output(untracked, env)
    return {REPO / name for name in names.split("\0") if name}


def build_settings(changes: Iterable[Path]) -> list[Path]:
    """Those of `changes` that every file's check depends on."""
    return sorted(
        path
        for path in changes
        if path.name in BUILD_SETTING_NAMES or path.suffix == BUILD_SETTING_SUFFIX
    )


def clang_format(files: Sequence[Path], env: Mapping[str, str]) -> None:
    if not files:
        print("+ clang-format: no changed files", flush=True)
        return
    print(f"+ clang-format over {len(files)} files", flush=True)
    process.run(["clang-format", "--dry-run", "--Werror", *files], env)


def built_inputs(cmake_dir: Path, env: Mapping[str, str]) -> set[Path]:
    """Every file the build's default target reads, so targets left out of it are not linted."""
    names = process.output(["ninja", "-C", cmake_dir, "-t", "inputs", "all"], env).splitlines()
    return {(cmake_dir / name).resolve() for name in names}


def compile_entries(cmake_dir: Path) -> list[dict[str, str]]:
    return json.loads((cmake_dir / "compile_commands.json").read_text())


def compiled_sources(cmake_dir: Path, env: Mapping[str, str]) -> list[Path]:
    """openSU's translation units in the build's compile_commands.json that the build compiles;
    none is an error."""
    require_own_tree(cmake_dir)
    own = [REPO / root for root in OWN_ROOTS]
    built = built_inputs(cmake_dir, env)
    files = {Path(entry["file"]).resolve() for entry in compile_entries(cmake_dir)}
    sources = sorted(
        path for path in files if any(path.is_relative_to(root) for root in own) and path in built
    )
    if not sources:
        raise RuntimeError(f"{cmake_dir}/compile_commands.json builds no sources of {REPO}")
    return sources


def parse_deps(text: str) -> dict[str, set[str]]:
    """`ninja -t deps` output as the files each output read, by the names ninja prints; an output
    whose record is stale has no entry."""
    records: dict[str, set[str]] = {}
    current: set[str] | None = None
    for line in text.splitlines():
        if line.startswith(DEPS_LINE_PREFIX):
            if current is not None:
                current.add(line.strip())
        elif line:
            match = DEPS_RECORD.fullmatch(line)
            current = records.setdefault(match.group(1), set()) if match else None
    return records


def includers(
    cmake_dir: Path, sources: Iterable[Path], headers: set[Path], env: Mapping[str, str]
) -> set[Path] | None:
    """Those of `sources` whose objects read any of `headers`, from the build's own dependency
    data; None when an object has no current record, which leaves that source unknown."""
    text = process.output(["ninja", "-C", cmake_dir, "-t", "deps"], env)
    records = {(cmake_dir / name).resolve(): read for name, read in parse_deps(text).items()}
    names = {name for read in records.values() for name in read}
    wanted = {name for name in names if (cmake_dir / name).resolve() in headers}
    known = set(sources)
    found = set()
    for entry in compile_entries(cmake_dir):
        source = Path(entry["file"]).resolve()
        if source not in known:
            continue
        read = records.get((cmake_dir / entry["output"]).resolve())
        if read is None:
            return None
        if read & wanted:
            found.add(source)
    return found


def affected_sources(cmake_dir: Path, changes: set[Path], env: Mapping[str, str]) -> list[Path]:
    """The translation units `changes` reach: the changed ones and every one that includes a
    changed header, or all of them, saying so, when the build's data cannot tell."""
    cxx = {path for path in changes if path.name.endswith(CXX_SUFFIXES)}
    if not cxx:
        return []
    sources = compiled_sources(cmake_dir, env)
    direct = cxx.intersection(sources)
    headers = cxx - direct
    found = includers(cmake_dir, sources, headers, env) if headers else set()
    if found is None:
        print(
            "+ ninja -t deps lacks a translation unit: clang-tidy takes the full list", flush=True
        )
        return sources
    return sorted(path for path in direct | found if path.is_file())


def clang_tidy(
    cmake_dir: Path,
    files: Sequence[Path],
    env: Mapping[str, str],
    jobs: int,
    extra: Sequence[str] = (),
) -> None:
    """`files` through clang-tidy (`.clang-tidy`: warnings are errors) on `jobs` processes, each of
    `extra` put before the compile command's own arguments."""
    if not files:
        print("+ clang-tidy: no changed translation units", flush=True)
        return

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


def ruff(files: Sequence[Path], env: Mapping[str, str]) -> None:
    if not files:
        print("+ ruff: no changed files", flush=True)
        return
    print(f"+ ruff over {len(files)} files", flush=True)
    process.run(["ruff", "check", *files], env)
    process.run(["ruff", "format", "--check", *files], env)


def run_checks(
    scope: Scope, cmake_dir: Path, env: Mapping[str, str], jobs: int, platform: str
) -> None:
    """Every check, on the Linux tree. A macOS or Windows tree runs clang-tidy alone, over its own
    translation units: the format and the Python are the same on every OS."""
    if platform == "windows":
        clang_tidy(cmake_dir, scope.translation_units, env, jobs, MSVC_STL_UNINLINED)
        return
    if platform == "macos":
        clang_tidy(cmake_dir, scope.translation_units, env, jobs)
        return
    clang_format(scope.cxx, env)
    ruff(scope.python, env)
    clang_tidy(cmake_dir, scope.translation_units, env, jobs)


def all_checks(cmake_dir: Path, env: Mapping[str, str], jobs: int, platform: str = "linux") -> None:
    """The full check: every tracked file and every translation unit the build compiles."""
    scope = Scope(
        tracked(env, CXX_SUFFIXES, OWN_ROOTS),
        tracked(env, (".py",)),
        compiled_sources(cmake_dir, env),
    )
    run_checks(scope, cmake_dir, env, jobs, platform)


def changed_checks(
    cmake_dir: Path, env: Mapping[str, str], jobs: int, platform: str, base: str
) -> None:
    """The checks over what changed against `base`; no change at all is a clean pass. The build's
    dependency data is the last build's, so build first."""
    changes = changed_files(base, env)
    if not changes:
        print(f"+ nothing changed against {base}: nothing to lint", flush=True)
        return
    settings = build_settings(changes)
    if settings:
        names = " ".join(path.relative_to(REPO).as_posix() for path in settings)
        print(f"+ build settings changed ({names}): the full list", flush=True)
        all_checks(cmake_dir, env, jobs, platform)
        return
    print(f"+ {len(changes)} files changed against {base}", flush=True)
    scope = Scope(
        within(changes, CXX_SUFFIXES, OWN_ROOTS),
        within(changes, (".py",)),
        affected_sources(cmake_dir, changes, env),
    )
    run_checks(scope, cmake_dir, env, jobs, platform)
