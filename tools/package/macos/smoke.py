"""The zipped app unpacked as a player's Mac unpacks it, checked, and run from there with its user
data in scratch folders: the shell draws a frame, and the runner draws the tests' fake core through
the MoltenVK the app carries."""

import shutil
import subprocess
from collections.abc import Mapping
from pathlib import Path

from tools.check import process
from tools.package import frame
from tools.package.macos import config


def player_environment(smoke: Path, env: Mapping[str, str]) -> dict[str, str]:
    """`env` with a scratch home and no Vulkan driver named, so the app finds its own."""
    home = smoke / "home"
    home.mkdir(parents=True, exist_ok=True)
    kept = {name: value for name, value in env.items() if not name.startswith(("VK_", "OPENSU_"))}
    return {**kept, "HOME": str(home)}


def run_logged(command: list[str | Path], log: Path, env: Mapping[str, str]) -> str:
    words = [str(word) for word in command]
    print("+ " + " ".join(words), flush=True)
    ran = subprocess.run(words, env=dict(env), capture_output=True, text=True, check=False,
                         timeout=300)  # fmt: skip
    output = ran.stdout + ran.stderr
    log.write_text(output)
    if ran.returncode != 0:
        raise RuntimeError(f"{words[0]} exited {ran.returncode}:\n{output}")
    return output


def run(layout: config.Layout, archive: Path, env: Mapping[str, str]) -> Path:
    if layout.smoke.exists():
        shutil.rmtree(layout.smoke)
    unpacked = layout.smoke / "app"
    unpacked.mkdir(parents=True)
    process.run(["ditto", "-x", "-k", archive, unpacked], env)
    app = unpacked / f"{config.APP_NAME}.app"
    process.run(["codesign", "--verify", "--strict", "--deep", "--verbose=2", app], env)
    user = player_environment(layout.smoke, env)
    opensu = app / "Contents" / "MacOS" / "opensu"
    usage = run_logged([opensu, "--help"], layout.smoke / "help.log", user)
    if "opensu" not in usage:
        raise RuntimeError(f"`opensu --help` printed no usage:\n{usage}")
    rendered = layout.smoke / "frame.png"
    run_logged([opensu, "--render", rendered], layout.smoke / "render.log", user)
    frame.check(rendered)
    game = layout.smoke / "game.bin"
    game.write_bytes(b"Bgame")
    folders = layout.smoke / "runner"
    folders.mkdir()
    drawn = layout.smoke / "runner.png"
    run_logged(
        [app / "Contents" / "MacOS" / "opensu-retro", "--core", layout.fake_core,
         "--saves", folders, "--system", folders, "--hidden", "--frames", "30",
         "--frame-png", drawn, game],
        layout.smoke / "runner.log", user,
    )  # fmt: skip
    if frame.png_size(drawn)[0] == 0:
        raise RuntimeError(f"{drawn} is empty")
    return rendered
