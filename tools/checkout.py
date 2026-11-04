"""The checkout every tool works on: the one the tools' own code is in.

`python -m tools.<name>` imports the tools from the current directory, ahead of uv's `--project` and
PYTHONPATH, so a run from one checkout in another checkout's environment works on the first one.
"""

import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]


def require_own_environment() -> None:
    """Refuse a run whose Python environment is another checkout's `.venv`."""
    environment = Path(sys.prefix).resolve()
    project = environment.parent
    if environment.name != ".venv" or not (project / "pyproject.toml").is_file():
        return
    if project != REPO:
        raise SystemExit(
            f"these tools are {REPO}'s but run in {project}'s environment; name the checkout "
            "to work on with `uv run --frozen --directory <checkout> python -m tools.<name>`"
        )
