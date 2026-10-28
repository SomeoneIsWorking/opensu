"""The stage (`cmake --install` of the default component) packed by Inno Setup into a per-user
installer, with its SHA-256 beside it."""

import shutil
from collections.abc import Mapping
from pathlib import Path

from tools import download
from tools.check import cmake, process
from tools.package.windows import config

# The `session` component is Linux's login session; everything else ships.
INSTALL_COMPONENT = "Unspecified"


def stage(plan: cmake.Plan, layout: config.Layout, env: Mapping[str, str]) -> Path:
    if layout.stage.exists():
        shutil.rmtree(layout.stage)
    cmake.install_to(plan, layout.stage, INSTALL_COMPONENT, env)
    for required in ("bin/opensu.exe", "bin/opensu-retro.exe", "bin/SDL3.dll",
                     "share/opensu"):  # fmt: skip
        if not (layout.stage / required).exists():
            raise RuntimeError(f"the stage has no {required}")
    return layout.stage


def pack(plan: cmake.Plan, layout: config.Layout, version: str, env: Mapping[str, str]) -> Path:
    staged = stage(plan, layout, env)
    if not config.ISCC.is_file():
        raise RuntimeError(f"Inno Setup 6 is not installed ({config.ISCC}): winget install "
                           "JRSoftware.InnoSetup")  # fmt: skip
    layout.out.mkdir(parents=True, exist_ok=True)
    target = layout.installer(version)
    process.run(
        [config.ISCC, "/Qp", f"/DAppVersion={version}", f"/DStageDir={staged}",
         f"/DOutputDir={layout.out}", f"/DOutputName={target.stem}",
         f"/DLicenseFile={config.REPO / 'LICENSE'}", config.SCRIPT],
        env,
    )  # fmt: skip
    if not target.is_file():
        raise RuntimeError(f"Inno Setup made no {target}")
    digest = download.sha256_file(target)
    # LF endings on every host, so `sha256sum -c` reads it on Linux.
    target.with_name(target.name + ".sha256").write_text(f"{digest}  {target.name}\n", newline="\n")
    return target
