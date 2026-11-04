"""The per-machine desktop shortcut: the application menu entry the install wrote under the prefix,
copied to the user's desktop, so both run the installed openSU. Never committed; `install` writes
it again each time."""

import shutil
import subprocess
from collections.abc import Mapping
from pathlib import Path

ENTRY = Path("share") / "applications" / "opensu.desktop"


def desktop_dir(env: Mapping[str, str]) -> Path:
    """The user's desktop folder, as xdg-user-dirs names it, else ~/Desktop."""
    if shutil.which("xdg-user-dir", path=env.get("PATH")) is not None:
        named = subprocess.run(
            ["xdg-user-dir", "DESKTOP"], env=dict(env), capture_output=True, text=True, check=True
        ).stdout.strip()
        if named:
            return Path(named)
    return Path.home() / "Desktop"


def place(prefix: Path, env: Mapping[str, str]) -> Path:
    """The installed menu entry copied to the desktop, executable as Plasma requires."""
    entry = prefix / ENTRY
    if not entry.is_file():
        raise RuntimeError(f"the install wrote no menu entry at {entry}")
    target = desktop_dir(env) / entry.name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(entry, target)
    target.chmod(0o755)
    print(f"+ desktop shortcut {target} from {entry}", flush=True)
    return target
