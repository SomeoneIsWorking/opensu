"""appimagetool over the AppDir: pinned static runtime, update information, optional signature."""

import shutil
from collections.abc import Mapping
from pathlib import Path

from tools.check import process
from tools.download import fetch_verified, sha256_file
from tools.package.appimage import config


def import_signing_key(armored: str, gnupg: Path, env: Mapping[str, str]) -> str:
    """Import the private key into a fresh keyring at `gnupg`; returns its fingerprint."""
    gnupg.mkdir(mode=0o700, parents=True, exist_ok=True)
    keyring_env = {**env, "GNUPGHOME": str(gnupg)}
    import_command = ["gpg", "--batch", "--quiet", "--import"]
    process.run_input(import_command, armored, keyring_env)
    listing = process.output(["gpg", "--batch", "--with-colons", "--list-secret-keys"], keyring_env)
    for line in listing.splitlines():
        if line.startswith("fpr:"):
            return line.split(":")[9]
    raise RuntimeError("the signing key imported no secret key")


def pack(layout: config.Layout, version: str, env: Mapping[str, str]) -> Path:
    """Write the AppImage and its .sha256 (and .zsync) to layout.out; returns the AppImage."""
    tool = fetch_verified(
        config.APPIMAGETOOL.url,
        config.APPIMAGETOOL.sha256,
        layout.downloads / config.APPIMAGETOOL.name,
    )
    tool.chmod(0o755)
    runtime = fetch_verified(
        config.RUNTIME.url, config.RUNTIME.sha256, layout.downloads / config.RUNTIME.name
    )
    layout.out.mkdir(parents=True, exist_ok=True)
    target = layout.appimage(version)
    target.unlink(missing_ok=True)
    # The tool is itself an AppImage; extracting it keeps FUSE out of the build container.
    tool_env = {**env, "APPIMAGE_EXTRACT_AND_RUN": "1", "ARCH": config.ARCH, "VERSION": version}
    command = [
        tool, "--no-appstream", "--runtime-file", runtime,
        "--updateinformation", config.UPDATE_INFORMATION,
    ]  # fmt: skip
    key = config.signing_key()
    if key:
        fingerprint = import_signing_key(key, layout.gnupg, env)
        tool_env["GNUPGHOME"] = str(layout.gnupg)
        command += ["--sign", "--sign-key", fingerprint]
    try:
        process.run([*command, layout.appdir, target], tool_env, cwd=layout.out)
    finally:
        if layout.gnupg.exists():
            process.run(["gpgconf", "--kill", "all"], {**env, "GNUPGHOME": str(layout.gnupg)})
            shutil.rmtree(layout.gnupg)
    checksum = target.with_name(target.name + ".sha256")
    checksum.write_text(f"{sha256_file(target)}  {target.name}\n")
    return target
