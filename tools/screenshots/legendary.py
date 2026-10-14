"""A stand-in `legendary` owning two Epic titles: one whose install logs 42% and then waits until
openSU stops it, and one installed, a build behind what Epic offers, whose uninstall waits the same
way, so the Downloads page shows a download and an uninstall in progress, and the installed game an
update, on every machine."""

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

# The installed title's version and the build Epic offers for its platform, as `list-installed
# --json` and `list --json` give them.
INSTALLED_VERSION = "1.0"
OFFERED_BUILD = "1.1"

OWNED = json.dumps(
    [
        {"app_name": APP_NAME, "app_title": TITLE},
        {
            "app_name": INSTALLED_APP_NAME,
            "app_title": INSTALLED_TITLE,
            "asset_infos": {"Windows": {"build_version": OFFERED_BUILD}},
        },
    ]
)
INSTALLED_COUNT = 1
INSTALLED = json.dumps(
    [
        {
            "app_name": INSTALLED_APP_NAME,
            "install_path": "/nonexistent/Demo",
            "version": INSTALLED_VERSION,
            "platform": "Windows",
        }
    ]
)

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
