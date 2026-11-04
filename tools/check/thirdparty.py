"""The pinned third-party libraries (PyroWave with Granite, librashader) in a shared prefix outside
every build tree: cmake/thirdparty built once per revision and toolchain, which openSU trees find
through `CMAKE_PREFIX_PATH`.

A prefix is named for the platform and a digest of what decides its contents: cmake/thirdparty,
cmake/ThirdParty.cmake (the pins) and cmake/toolchains, the toolchain's CMake arguments, each
compiler's `--version` and `rustc -vV`. It is built in a directory of its own beside it and moved
into place whole, so a reader never sees half of one and two builds of the same one cannot mix.
"""

import hashlib
import os
import shutil
import subprocess
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path

from tools.check import process
from tools.checkout import REPO

SOURCE = REPO / "cmake" / "thirdparty"
PINS = REPO / "cmake" / "ThirdParty.cmake"
TOOLCHAINS = REPO / "cmake" / "toolchains"
DEFAULT_ROOT = Path.home() / ".local" / "share" / "opensu-thirdparty"
# Written last, so its presence says the prefix is whole.
COMPLETE = "complete.txt"


@dataclass(frozen=True)
class Toolchain:
    """What a prefix is built with: the platform, the CMake arguments naming the compilers or
    toolchain file, and the compilers whose versions it depends on."""

    platform: str
    arguments: Sequence[str]
    compilers: Sequence[str] = ()


def _version(command: Sequence[str], env: Mapping[str, str]) -> str:
    """`command`'s output, or the reason it has none; a missing tool fails later, in the build."""
    try:
        result = subprocess.run(
            list(command), env=dict(env), capture_output=True, text=True, check=False
        )
    except OSError as error:
        return f"{command[0]}: {error.strerror}"
    return result.stdout + result.stderr


def identity(toolchain: Toolchain, env: Mapping[str, str]) -> str:
    """Everything a prefix's contents depend on, one item per line."""
    # Paths into the checkout are named relative to it, so every checkout shares one prefix.
    arguments = [arg.replace(str(REPO), "<checkout>") for arg in toolchain.arguments]
    lines = [f"platform {toolchain.platform}", *(f"argument {arg}" for arg in arguments)]
    files = [PINS, *(p for d in (SOURCE, TOOLCHAINS) for p in d.rglob("*") if p.is_file())]
    for path in sorted(files):
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        lines.append(f"file {path.relative_to(REPO).as_posix()} {digest}")
    for compiler in toolchain.compilers:
        lines.append(f"compiler {compiler}: {_version([compiler, '--version'], env).strip()}")
    lines.append(f"rustc: {_version(['rustc', '-vV'], env).strip()}")
    return "\n".join(lines) + "\n"


def prefix(toolchain: Toolchain, env: Mapping[str, str], root: Path = DEFAULT_ROOT) -> Path:
    """The prefix `toolchain` builds, whether or not it is built yet."""
    digest = hashlib.sha256(identity(toolchain, env).encode()).hexdigest()[:16]
    return root / f"{toolchain.platform}-{digest}"


def require(toolchain: Toolchain, env: Mapping[str, str], root: Path = DEFAULT_ROOT) -> Path:
    """The built prefix; refused when `deps` has not built it."""
    path = prefix(toolchain, env, root)
    if not (path / COMPLETE).is_file():
        raise RuntimeError(f"no third-party prefix at {path}; `tools.check deps` builds it")
    return path


def ensure(
    toolchain: Toolchain, env: Mapping[str, str], jobs: int, root: Path = DEFAULT_ROOT
) -> Path:
    """The prefix for `toolchain`, built first when it is missing."""
    path = prefix(toolchain, env, root)
    if (path / COMPLETE).is_file():
        print(f"+ third-party prefix {path} (built)", flush=True)
        return path
    work = root / f"{path.name}.{os.getpid()}"
    build_dir = work / "build"
    stage = work / "prefix"
    if work.exists():
        shutil.rmtree(work)
    build_env = {**env, "CARGO_BUILD_JOBS": str(jobs)}
    process.run(
        [
            "cmake", "-S", SOURCE, "-B", build_dir, "-G", "Ninja",
            "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_INSTALL_PREFIX={stage}",
            *toolchain.arguments,
        ],
        build_env,
    )  # fmt: skip
    process.run(["cmake", "--build", build_dir, "--parallel", str(jobs)], build_env)
    process.run(["cmake", "--install", build_dir], build_env)
    (stage / COMPLETE).write_text(identity(toolchain, env))
    try:
        stage.rename(path)
    except OSError:
        # Another build of the same prefix finished first; its copy is the same.
        if not (path / COMPLETE).is_file():
            raise
    shutil.rmtree(work)
    print(f"+ third-party prefix {path}", flush=True)
    return path
