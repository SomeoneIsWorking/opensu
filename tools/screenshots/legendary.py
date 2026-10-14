"""A stand-in `legendary` owning two Epic titles: one whose install logs 42% and then waits until
openSU stops it, and one installed whose uninstall waits the same way, so the Downloads page shows a
download and an uninstall in progress on every machine."""

import json
from pathlib import Path

APP_NAME = "Demo"
TITLE = "Demo Epic Game"
INSTALLED_APP_NAME = "DemoInstalled"
INSTALLED_TITLE = "Demo Installed Game"
PERCENT = 42
# A line as legendary's download manager logs it.
PROGRESS = (
    f"[DLManager] INFO: = Progress: {PERCENT}.00% ({PERCENT}/100), "
    "Running for 00:00:10, ETA: 00:00:14"
)

OWNED = json.dumps(
    [
        {"app_name": APP_NAME, "app_title": TITLE},
        {"app_name": INSTALLED_APP_NAME, "app_title": INSTALLED_TITLE},
    ]
)
INSTALLED_COUNT = 1
INSTALLED = json.dumps([{"app_name": INSTALLED_APP_NAME, "install_path": "/nonexistent/Demo"}])

SCRIPT = f"""#!/bin/sh
case "$1" in
list) echo '{OWNED}' ;;
list-installed) echo '{INSTALLED}' ;;
install)
    echo "{PROGRESS}" >&2
    exec sleep 600
    ;;
uninstall) exec sleep 600 ;;
*) exit 1 ;;
esac
"""


def install(tools: Path) -> Path:
    """Writes the stand-in into `tools`, the directory the run puts first on PATH."""
    tools.mkdir(parents=True, exist_ok=True)
    legendary = tools / "legendary"
    legendary.write_text(SCRIPT)
    legendary.chmod(0o755)
    return tools
