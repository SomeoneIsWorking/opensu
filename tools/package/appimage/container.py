"""The build container: its image, tagged by what defines it, and running a command inside it."""

from collections.abc import Mapping, Sequence
from pathlib import Path

from tools import podman
from tools.check import process
from tools.package.appimage import config


def ensure_image(env: Mapping[str, str]) -> str:
    """The build image, built from the Containerfile unless it already exists."""
    tag = podman.content_tag(
        config.IMAGE_NAME, config.CONTAINERFILE.read_bytes(), config.UV_VERSION, config.UV.sha256
    )
    return podman.ensure_image(
        tag,
        env,
        containerfile=config.CONTAINERFILE,
        build_args=(f"UV_VERSION={config.UV_VERSION}", f"UV_SHA256={config.UV.sha256}"),
    )


def run_in(
    tag: str,
    command: Sequence[str | Path],
    layout: config.Layout,
    env: Mapping[str, str],
    passthrough: Sequence[str] = (),
) -> None:
    """Run `command` in the build image with the checkout mounted at its own path, as this user.

    `passthrough` names variables handed to the container by name, so their values never appear
    on a command line.
    """
    home = layout.root / "home"
    home.mkdir(parents=True, exist_ok=True)
    words: list[str | Path] = [
        "podman", "run", "--rm", "--userns=keep-id", "--security-opt", "label=disable",
        "-v", f"{config.REPO}:{config.REPO}", "-w", config.REPO,
        "-e", f"HOME={home}",
        "-e", f"UV_CACHE_DIR={layout.root / 'uv-cache'}",
        "-e", f"UV_PROJECT_ENVIRONMENT={layout.root / 'venv'}",
    ]  # fmt: skip
    common = podman.git_common_dir(config.REPO)
    if common is not None:
        words += ["-v", f"{common}:{common}:ro"]
    for name in passthrough:
        if name in env:
            words += ["-e", name]
    process.run([*words, tag, *command], env)
