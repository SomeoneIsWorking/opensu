"""The installer run as a player runs it, silently and per user, into a scratch folder, and the
installed openSU drawing one frame with its user data in scratch folders too."""

import shutil
import subprocess
from collections.abc import Mapping
from pathlib import Path

from tools.check import process
from tools.package import frame
from tools.package.windows import config


def run(layout: config.Layout, installer: Path, env: Mapping[str, str]) -> Path:
    if layout.smoke.exists():
        shutil.rmtree(layout.smoke)
    app = layout.smoke / "app"
    appdata = layout.smoke / "appdata"
    local = layout.smoke / "localappdata"
    for folder in (appdata, local):
        folder.mkdir(parents=True)
    process.run(
        [installer, "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/CURRENTUSER",
         f"/DIR={app}", f"/LOG={layout.smoke / 'install.log'}"],
        env,
    )  # fmt: skip
    opensu = app / "bin" / "opensu.exe"
    if not opensu.is_file():
        raise RuntimeError(f"the installer put no {opensu}")
    user = {**env, "APPDATA": str(appdata), "LOCALAPPDATA": str(local)}
    usage = subprocess.run([opensu, "--help"], env=user, capture_output=True, text=True,
                           check=False, timeout=60)  # fmt: skip
    if "opensu" not in (usage.stdout + usage.stderr):
        raise RuntimeError(f"`opensu.exe --help` printed no usage:\n{usage.stdout}{usage.stderr}")
    rendered = layout.smoke / "frame.png"
    process.run([opensu, "--render", rendered], user)
    frame.check(rendered)
    return rendered
