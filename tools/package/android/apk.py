"""The built APK checked before it leaves: every ABI's libraries, the assets and the release key."""

import zipfile
from pathlib import Path
from types import ModuleType

from tools.package.android import config

LIBRARIES = ("libmain.so", "libopensu-retro.so", "libSDL3.so", "libc++_shared.so")


def inspect(apk: Path, layout: config.Layout, port: ModuleType) -> str:
    """The signer's SHA-256; refuses a missing library or asset, or a key not the release's."""
    with zipfile.ZipFile(apk) as archive:
        names = set(archive.namelist())
    required = {f"lib/{abi}/{name}" for abi in config.ABIS for name in LIBRARIES}
    required |= {"assets/opensu/emulators.json", "assets/opensu/CalSans-Regular.ttf"}
    missing = sorted(required - names)
    if missing:
        raise config.refuse(f"{apk.name} lacks " + ", ".join(missing))
    signer = port.verify_apk_signature(apk, layout.sdk, config.BUILD_TOOLS, config.API_LEVEL)
    if signer.upper() != config.RELEASE_CERT_SHA256:
        raise config.refuse(f"{apk.name} is signed by {signer}, not the release key")
    return signer
