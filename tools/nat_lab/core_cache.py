"""mGBA (built-in) as openSU downloads it, once per catalog pin, for the lab's shells.

The lab's namespaces have no internet, so a hidden openSU outside them downloads the core through
Settings › Emulators › Emulators and cores: the catalog's pinned release, checked as the product
checks it. Each lab shell gets a copy of what that download left in its data dir.
"""

import hashlib
import json
import time
from pathlib import Path

from tools.screenshots import scenes
from tools.screenshots.shell import HiddenShell, Profile

ENTRY = "mgba-core"
CORE = "mgba_libretro"


def release_key(repo: Path) -> str:
    """A short hash of the catalog's pinned release of mGBA (built-in)."""
    catalog = json.loads((repo / "assets" / "emulators.json").read_text())
    entries = catalog["emulators"] if isinstance(catalog, dict) else catalog
    entry = next(each for each in entries if each.get("id") == ENTRY)
    text = json.dumps(entry["release"], sort_keys=True)
    return hashlib.sha256(text.encode()).hexdigest()[:16]


def cores(repo: Path, build: Path, root: Path) -> Path:
    """The folder of the downloaded core and its record, downloaded now unless already kept."""
    folder = root / release_key(repo)
    kept = folder / "home" / ".local" / "share" / "opensu" / "cores"
    if (kept / f"{CORE}.json").exists():
        return kept
    home = folder / "home"
    (folder / "steam").mkdir(parents=True, exist_ok=True)
    (folder / "roms").mkdir(exist_ok=True)
    profile = Profile(home=home, steam_root=folder / "steam", rom_root=folder / "roms")
    with HiddenShell(build / "src" / "opensu", profile, folder / "shell.log") as shell:
        scenes._settings_page("Emulators")(shell)
        shell.press("a")
        walk(shell, "emulators.page.downloads")
        shell.press("a")
        walk(shell, f"emulators.run.{ENTRY}")
        shell.press("a")
        until = time.monotonic() + 300
        while not (kept / f"{CORE}.json").exists():
            if time.monotonic() > until:
                raise RuntimeError(f"mGBA (built-in) never downloaded; see {folder / 'shell.log'}")
            time.sleep(0.5)
    return kept


def walk(shell: HiddenShell, target: str, most: int = 80) -> None:
    for _ in range(most):
        if shell.state().get("screenFocus") == target:
            return
        shell.press("down")
    raise RuntimeError(f"never reached {target}: {shell.state().get('screenFocus')}")
