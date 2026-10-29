"""openSU.app from the stage: `cmake --install` of the default component into Contents, the dylibs
it links copied into Frameworks, MoltenVK as its Vulkan driver, ad-hoc signed and zipped with its
SHA-256 beside it."""

import json
import shutil
import subprocess
from collections.abc import Mapping
from pathlib import Path

from tools import download
from tools.check import cmake, macos, process
from tools.package.macos import config

# The `session` component is Linux's login session; everything else ships.
INSTALL_COMPONENT = "Unspecified"
# What every Mac has; anything else a binary links must be inside the app.
SYSTEM_PREFIXES = ("/usr/lib/", "/System/Library/")


def linked(binary: Path, env: Mapping[str, str]) -> list[str]:
    """The libraries `binary` loads, as `otool -L` names them, without its own id."""
    lines = process.output(["otool", "-L", binary], env).splitlines()[1:]
    names = [line.strip().split(" (compatibility")[0] for line in lines if line.strip()]
    own = process.output(["otool", "-D", binary], env).splitlines()[1:]
    return [name for name in names if name not in own]


def carry_libraries(contents: Path, libraries: Path, env: Mapping[str, str]) -> None:
    """Every dylib the executables and Frameworks link, followed to the end, copied from
    `libraries` into Frameworks; refuses a library from anywhere else."""
    frameworks = contents / "Frameworks"
    pending = [contents / "MacOS" / name for name in config.EXECUTABLES]
    pending += sorted(frameworks.glob("*.dylib"))
    seen: set[Path] = set()
    while pending:
        binary = pending.pop()
        if binary in seen:
            continue
        seen.add(binary)
        for name in linked(binary, env):
            if name.startswith(SYSTEM_PREFIXES):
                continue
            if not name.startswith("@rpath/"):
                raise RuntimeError(f"{binary.name} links {name}, which a player's Mac lacks")
            leaf = name.removeprefix("@rpath/")
            carried = frameworks / leaf
            if not carried.is_file():
                source = libraries / leaf
                if not source.is_file():
                    raise RuntimeError(f"{binary.name} links {name}, which {libraries} lacks")
                shutil.copy2(source.resolve(), carried)
            pending.append(carried)


def add_moltenvk(contents: Path, root: Path) -> None:
    """MoltenVK in Frameworks and its ICD manifest where the loader looks inside a bundle."""
    icd = macos.moltenvk(root)
    shutil.copy2(icd.parent / "libMoltenVK.dylib", contents / "Frameworks" / "libMoltenVK.dylib")
    manifest = json.loads(icd.read_text())
    manifest["ICD"]["library_path"] = "../../../Frameworks/libMoltenVK.dylib"
    folder = contents / config.ICD_FOLDER
    folder.mkdir(parents=True, exist_ok=True)
    (folder / icd.name).write_text(json.dumps(manifest, indent=4) + "\n")


def write_info(contents: Path, version: str) -> None:
    text = config.INFO_PLIST.read_text()
    text = text.replace("@VERSION@", version).replace("@MINIMUM_SYSTEM@", macos.DEPLOYMENT_TARGET)
    (contents / "Info.plist").write_text(text)


def sign(app: Path, env: Mapping[str, str]) -> None:
    """Ad-hoc signatures, inside out: every library, then the executables, then the app."""
    contents = app / "Contents"
    inner = sorted((contents / "Frameworks").glob("*.dylib"))
    inner += [contents / "MacOS" / name for name in config.EXECUTABLES]
    for binary in inner:
        process.run(["codesign", "--force", "--sign", "-", "--timestamp=none", binary], env)
    process.run(["codesign", "--force", "--sign", "-", "--timestamp=none", app], env)
    process.run(["codesign", "--verify", "--strict", "--deep", "--verbose=2", app], env)


def stage(plan: cmake.Plan, layout: config.Layout, version: str, env: Mapping[str, str]) -> Path:
    if layout.stage.exists():
        shutil.rmtree(layout.stage)
    cmake.install_to(plan, layout.contents, INSTALL_COMPONENT, env)
    for name in config.EXECUTABLES:
        if not (layout.contents / "MacOS" / name).is_file():
            raise RuntimeError(f"the stage has no Contents/MacOS/{name}")
    for required in ("Resources/opensu", "Frameworks/librashader.dylib"):
        if not (layout.contents / required).exists():
            raise RuntimeError(f"the stage has no Contents/{required}")
    write_info(layout.contents, version)
    carry_libraries(layout.contents, layout.libraries, env)
    add_moltenvk(layout.contents, plan.build_dir / "moltenvk")
    sign(layout.app, env)
    return layout.app


def pack(plan: cmake.Plan, layout: config.Layout, version: str, env: Mapping[str, str]) -> Path:
    app = stage(plan, layout, version, env)
    layout.out.mkdir(parents=True, exist_ok=True)
    target = layout.archive(version)
    target.unlink(missing_ok=True)
    # ditto keeps the signatures' extended attributes and the symlinks, as Finder's Compress does.
    subprocess.run(
        ["ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", str(app), str(target)],
        env=dict(env),
        check=True,
    )
    digest = download.sha256_file(target)
    target.with_name(target.name + ".sha256").write_text(f"{digest}  {target.name}\n")
    return target
