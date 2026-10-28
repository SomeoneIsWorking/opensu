"""The AppDir: openSU's install tree plus what the AppImage format needs at its top."""

import shutil
from pathlib import Path

from tools.appimage.config import DESKTOP_ID


def finish(appdir: Path) -> None:
    """AppRun, the desktop entry and the icon at the AppDir's top, from the installed tree."""
    usr = appdir / "usr"
    executable = usr / "bin" / "opensu"
    desktop = usr / "share" / "applications" / f"{DESKTOP_ID}.desktop"
    icon = usr / "share" / "icons" / "hicolor" / "scalable" / "apps" / f"{DESKTOP_ID}.svg"
    for path in (executable, desktop, icon):
        if not path.is_file():
            raise RuntimeError(f"the install tree lacks {path.relative_to(appdir)}")
    # A symlink keeps /proc/self/exe on usr/bin/opensu, which every relative path starts from.
    apprun = appdir / "AppRun"
    apprun.unlink(missing_ok=True)
    apprun.symlink_to("usr/bin/opensu")
    shutil.copy2(desktop, appdir / desktop.name)
    shutil.copy2(icon, appdir / icon.name)
    dir_icon = appdir / ".DirIcon"
    dir_icon.unlink(missing_ok=True)
    dir_icon.symlink_to(icon.name)
