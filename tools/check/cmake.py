"""Configuring, building, testing and installing openSU with CMake and Clang."""

import re
from collections.abc import Mapping, Sequence
from dataclasses import dataclass, field
from pathlib import Path

from tools.check import process
from tools.check.deps import Deps

REPO = Path(__file__).resolve().parents[2]

GAMESCOPE_MODES = ("podman", "native", "off")


@dataclass(frozen=True)
class Plan:
    """One build tree: where it lives, what it builds against and how the fork is built."""

    build_dir: Path
    deps: Deps
    gamescope: str = "podman"
    build_type: str = "RelWithDebInfo"
    install_prefix: str = "/usr/local"
    defines: Sequence[str] = field(default_factory=tuple)

    @property
    def cmake_dir(self) -> Path:
        return self.build_dir / "cmake"

    @property
    def scratch(self) -> Path:
        return self.build_dir / "tmp"

    def configure_args(self) -> list[str]:
        if self.gamescope not in GAMESCOPE_MODES:
            raise ValueError(f"gamescope must be one of {GAMESCOPE_MODES}, not {self.gamescope}")
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
            f"-DRAYLIB_ROOT={self.deps.raylib_prefix}",
            f"-DLUCENT_ROOT={self.deps.lucent}",
            f"-DGPU2D_ROOT={self.deps.gpu2d}",
        ]
        if self.gamescope == "off":
            args.append("-DOPENSU_BUILD_GAMESCOPE=OFF")
        else:
            args += [
                "-DOPENSU_BUILD_GAMESCOPE=ON",
                f"-DOPENSU_GAMESCOPE_BUILD_ENV={self.gamescope}",
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


def configure(plan: Plan, env: Mapping[str, str]) -> None:
    process.run(plan.configure_args(), env)
    found = compiler_id(plan.cmake_dir)
    if found != "Clang":
        raise RuntimeError(
            f"{plan.cmake_dir} is configured with {found or 'no'} C++ compiler, not Clang"
        )


def build(plan: Plan, env: Mapping[str, str]) -> None:
    process.run(["cmake", "--build", plan.cmake_dir], env)


def test(plan: Plan, env: Mapping[str, str]) -> None:
    process.run(["ctest", "--test-dir", plan.cmake_dir, "--output-on-failure"], env)


def install(plan: Plan, destdir: Path, component: str, env: Mapping[str, str]) -> None:
    """Install one component under `destdir`, stripped."""
    process.run(
        ["cmake", "--install", plan.cmake_dir, "--component", component, "--strip"],
        {**env, "DESTDIR": str(destdir)},
    )
