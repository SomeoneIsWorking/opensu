"""Where openSU keeps its settings under a home, as src/config/locations resolves it per host."""

import sys
from pathlib import Path


def config_dir(home: Path) -> Path:
    """openSU's config directory under `home`, with XDG_CONFIG_HOME at home/.config on Linux."""
    if sys.platform == "darwin":
        return home / "Library" / "Application Support" / "openSU"
    return home / ".config" / "opensu"
