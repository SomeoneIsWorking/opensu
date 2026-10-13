"""A stand-in `wpctl` at a fixed volume, so the volume display and the Audio page read the same
on every machine. openSU --hidden never writes to the mixer, so only reads need answering."""

from pathlib import Path

PERCENT = 50

SCRIPT = f"""#!/bin/sh
if [ "$1" = get-volume ]; then
    echo "Volume: {PERCENT / 100:.2f}"
    exit 0
fi
exit 1
"""


def install(tools: Path) -> Path:
    """Writes the stand-in into `tools`, the directory the run puts first on PATH."""
    tools.mkdir(parents=True, exist_ok=True)
    wpctl = tools / "wpctl"
    wpctl.write_text(SCRIPT)
    wpctl.chmod(0o755)
    return tools
