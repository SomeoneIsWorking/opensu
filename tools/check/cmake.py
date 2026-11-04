"""Configuring, building, testing and installing openSU with CMake and Clang."""

import os
import platform
import re
from collections.abc import Mapping, Sequence
from dataclasses import dataclass, field
from pathlib import Path

from tools.check import deps, macos, process, thirdparty, windows
from tools.check.deps import Deps
from tools.checkout import REPO

GAMESCOPE_MODES = ("native", "off")
PLATFORMS = ("linux", "windows", "macos")
DEFAULT_JOBS = os.cpu_count() or 1


@dataclass(frozen=True)
class Plan:
    """One build tree: where it lives, the OS it builds for, what it builds against, how the
    fork is built (Linux only) and how many compilers or clang-tidy processes run at once."""

    build_dir: Path
    deps: Deps
    platform: str = "linux"
    gamescope: str = "native"
    build_type: str = "RelWithDebInfo"
    install_prefix: str = "/usr/local"
    defines: Sequence[str] = field(default_factory=tuple)
    jobs: int = DEFAULT_JOBS

    @property
    def cmake_dir(self) -> Path:
        return self.build_dir / "cmake"

    @property
    def scratch(self) -> Path:
        return self.build_dir / "tmp"

    def toolchain(self, env: Mapping[str, str]) -> thirdparty.Toolchain:
        """The compilers the shared third-party prefix this tree uses is built with."""
        if self.platform == "windows":
            return thirdparty.Toolchain(
                "windows-x64", [f"-DCMAKE_TOOLCHAIN_FILE={windows.TOOLCHAIN}"], ["clang-cl"]
            )
        if self.platform == "macos":
            defines = macos.compiler_defines(env)
            compilers = [d.split("=", 1)[1] for d in defines if "_COMPILER=" in d]
            return thirdparty.Toolchain("macos-arm64", defines, compilers)
        return thirdparty.Toolchain(
            f"linux-{platform.machine()}",
            ["-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++"],
            ["clang", "clang++"],
        )

    def configure_args(self, env: Mapping[str, str]) -> list[str]:
        if self.platform not in PLATFORMS:
            raise ValueError(f"platform must be one of {PLATFORMS}, not {self.platform}")
        if self.gamescope not in GAMESCOPE_MODES:
            raise ValueError(f"gamescope must be one of {GAMESCOPE_MODES}, not {self.gamescope}")
        found = f"-DCMAKE_PREFIX_PATH={thirdparty.require(self.toolchain(env), env)}"
        if self.platform == "windows":
            return [
                "cmake", "-S", str(REPO), "-B", str(self.cmake_dir), "-G", "Ninja",
                f"-DCMAKE_BUILD_TYPE={self.build_type}",
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                f"-DLUCENT_ROOT={self.deps.lucent}",
                f"-DGPU2D_ROOT={self.deps.gpu2d}",
                found,
                *windows.configure_defines(self.deps.vcpkg, self.build_dir),
                *self.defines,
            ]  # fmt: skip
        if self.platform == "macos":
            return [
                "cmake", "-S", str(REPO), "-B", str(self.cmake_dir), "-G", "Ninja",
                f"-DCMAKE_BUILD_TYPE={self.build_type}",
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                f"-DLUCENT_ROOT={self.deps.lucent}",
                f"-DGPU2D_ROOT={self.deps.gpu2d}",
                found,
                *macos.configure_defines(self.deps.vcpkg, self.build_dir, env),
                *self.defines,
            ]  # fmt: skip
        args = [
            "cmake",
            "-S",
            str(REPO),
            "-B",
            str(self.cmake_dir),
            "-G",
            "Ninja",
            "-DCMAKE_CXX_COMPILER=clang++",
            f"-DCMAKE_BUILD_TYPE={self.build_type}",
            f"-DCMAKE_INSTALL_PREFIX={self.install_prefix}",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            f"-DLUCENT_ROOT={self.deps.lucent}",
            f"-DGPU2D_ROOT={self.deps.gpu2d}",
            found,
        ]
        if self.gamescope == "off":
            args.append("-DOPENSU_BUILD_GAMESCOPE=OFF")
        else:
            args += [
                "-DOPENSU_BUILD_GAMESCOPE=ON",
                f"-DOPENSU_GAMESCOPE_BUILD_ENV={self.gamescope}",
                f"-DOPENSU_GAMESCOPE_JOBS={self.jobs}",
            ]
        args += list(self.defines)
        return args


def compiler_id(cmake_dir: Path) -> str:
    """CMAKE_CXX_COMPILER_ID of a configured tree."""
    for path in (cmake_dir / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"):
        match = re.search(r'set\(CMAKE_CXX_COMPILER_ID "([^"]*)"\)', path.read_text())
        if match:
            return match.group(1)
    return ""


def configured_source(cmake_dir: Path) -> Path | None:
    """The source directory a configured tree was generated from, None before configure."""
    cache = cmake_dir / "CMakeCache.txt"
    if not cache.is_file():
        return None
    match = re.search(r"^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$", cache.read_text(), re.MULTILINE)
    return Path(match.group(1)).resolve() if match else None


def require_own_tree(cmake_dir: Path) -> None:
    """Refuse a tree configured from another checkout, whose build, lint and tests are not this
    checkout's."""
    source = configured_source(cmake_dir)
    if source is not None and source != REPO:
        raise RuntimeError(f"{cmake_dir} is configured from {source}, not from {REPO}")


def prepare(plan: Plan, env: Mapping[str, str]) -> None:
    """The pinned sources, then the shared third-party prefix built with the tree's compilers."""
    deps.fetch(plan.deps, env, plan.platform)
    thirdparty.ensure(plan.toolchain(env), env, plan.jobs)


def configure(plan: Plan, env: Mapping[str, str]) -> None:
    process.run(plan.configure_args(env), env)
    found = compiler_id(plan.cmake_dir)
    if found != "Clang":
        raise RuntimeError(
            f"{plan.cmake_dir} is configured with {found or 'no'} C++ compiler, not Clang"
        )


def build(plan: Plan, env: Mapping[str, str]) -> None:
    # cargo, which builds librashader under one ninja job, would otherwise start a job per core.
    process.run(
        ["cmake", "--build", plan.cmake_dir, "--parallel", str(plan.jobs)],
        {**env, "CARGO_BUILD_JOBS": str(plan.jobs)},
    )


def test(plan: Plan, env: Mapping[str, str]) -> None:
    if plan.platform == "windows":
        env = windows.lavapipe(plan.build_dir / "lavapipe", env)
    if plan.platform == "macos":
        env = macos.with_moltenvk(plan.build_dir / "moltenvk", env)
    process.run(["ctest", "--test-dir", plan.cmake_dir, "--output-on-failure"], env)


def install_to(plan: Plan, prefix: Path, component: str, env: Mapping[str, str]) -> None:
    """Install one component into `prefix` as it is, for a package that is its own prefix."""
    process.run(
        ["cmake", "--install", plan.cmake_dir, "--component", component, "--prefix", prefix], env
    )


def install(plan: Plan, destdir: Path, component: str, env: Mapping[str, str]) -> None:
    """Install one component under `destdir`, stripped."""
    process.run(
        ["cmake", "--install", plan.cmake_dir, "--component", component, "--strip"],
        {**env, "DESTDIR": str(destdir)},
    )
