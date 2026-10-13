"""Container images the tools build, tagged by the content that defines them."""

import hashlib
import shlex
import subprocess
from collections.abc import Mapping, Sequence
from pathlib import Path


def content_tag(name: str, *parts: str | bytes) -> str:
    """`name:<12 hex>` of the parts: a changed recipe gets a new image, an unchanged one none."""
    digest = hashlib.sha256()
    for part in parts:
        digest.update(part.encode() if isinstance(part, str) else part)
        digest.update(b"\0")
    return f"{name}:{digest.hexdigest()[:12]}"


def ensure_image(
    tag: str,
    env: Mapping[str, str],
    containerfile: Path | None = None,
    recipe: str = "",
    build_args: Sequence[str] = (),
) -> str:
    """Build `tag` from `containerfile` (or the `recipe` text) unless it already exists."""
    exists = subprocess.run(["podman", "image", "exists", tag], env=dict(env), check=False)
    if exists.returncode == 0:
        return tag
    words = ["podman", "build", "-t", tag]
    for arg in build_args:
        words += ["--build-arg", arg]
    if containerfile is not None:
        words += ["-f", str(containerfile), str(containerfile.parent)]
        print("+ " + shlex.join(words), flush=True)
        subprocess.run(words, env=dict(env), check=True)
    else:
        words += ["-f", "-"]
        print("+ " + shlex.join(words) + " < (recipe)", flush=True)
        subprocess.run(words, env=dict(env), input=recipe, text=True, check=True)
    return tag


def git_common_dir(repo: Path) -> Path | None:
    """The main repository's .git for a worktree checkout, which a container must also mount."""
    dot_git = repo / ".git"
    if not dot_git.is_file():
        return None
    gitdir = Path(dot_git.read_text().removeprefix("gitdir:").strip())
    return (gitdir / (gitdir / "commondir").read_text().strip()).resolve()


def display_words(env: Mapping[str, str]) -> list[str]:
    """`podman run` words giving a container the caller's X display (CI's is xvfb-run's)."""
    display = env.get("DISPLAY", "")
    if not display:
        raise RuntimeError("hidden windows need DISPLAY (CI wraps the job in xvfb-run)")
    words = ["-v", "/tmp/.X11-unix:/tmp/.X11-unix:ro", "-e", f"DISPLAY={display}"]
    xauthority = env.get("XAUTHORITY", "")
    if xauthority:
        # Cookies are keyed by hostname, so the container keeps the host's.
        words += ["--uts", "host"]
        words += ["-v", f"{xauthority}:/xauthority:ro", "-e", "XAUTHORITY=/xauthority"]
    return words
