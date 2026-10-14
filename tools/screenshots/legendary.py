"""A stand-in `legendary` owning one Epic title, whose install logs 42% and then waits until
openSU stops it, so the Downloads page shows a download in progress on every machine."""

from pathlib import Path

APP_NAME = "Demo"
TITLE = "Demo Epic Game"
PERCENT = 42
# A line as legendary's download manager logs it.
PROGRESS = (
    f"[DLManager] INFO: = Progress: {PERCENT}.00% ({PERCENT}/100), "
    "Running for 00:00:10, ETA: 00:00:14"
)

SCRIPT = f"""#!/bin/sh
case "$1" in
list) echo '[{{"app_name": "{APP_NAME}", "app_title": "{TITLE}"}}]' ;;
list-installed) echo '[]' ;;
install)
    echo "{PROGRESS}" >&2
    exec sleep 600
    ;;
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
