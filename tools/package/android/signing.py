"""The release key for Gradle: the base64 keystore from OPENSU_ANDROID_KEYSTORE written to an
owner-only file, or OPENSU_ANDROID_KEYSTORE_FILE as it is."""

import base64
import binascii
import os
from collections.abc import Mapping
from pathlib import Path

from tools.package.android import config


def gradle_environment(env: Mapping[str, str], scratch: Path) -> dict[str, str]:
    """The Gradle variables for a signed release; refuses when any part of the key is missing."""
    missing = [name for name in config.SIGNING_ENV[1:] if not env.get(name)]
    keystore_file = env.get("OPENSU_ANDROID_KEYSTORE_FILE", "")
    encoded = env.get("OPENSU_ANDROID_KEYSTORE", "")
    if not keystore_file and not encoded:
        missing.insert(0, "OPENSU_ANDROID_KEYSTORE (or OPENSU_ANDROID_KEYSTORE_FILE)")
    if missing:
        raise config.refuse("a release APK needs " + ", ".join(missing))
    if not keystore_file:
        try:
            decoded = base64.b64decode(encoded, validate=True)
        except binascii.Error as error:
            raise config.refuse("OPENSU_ANDROID_KEYSTORE is not base64") from error
        scratch.mkdir(parents=True, exist_ok=True)
        target = scratch / "release.keystore"
        descriptor = os.open(target, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        with os.fdopen(descriptor, "wb") as out:
            out.write(decoded)
        keystore_file = str(target)
    return {
        "OPENSU_ANDROID_KEYSTORE_FILE": keystore_file,
        "OPENSU_ANDROID_KEYSTORE_PASSWORD": env["OPENSU_ANDROID_KEYSTORE_PASSWORD"],
        "OPENSU_ANDROID_KEY_ALIAS": env["OPENSU_ANDROID_KEY_ALIAS"],
        "OPENSU_ANDROID_KEY_PASSWORD": env["OPENSU_ANDROID_KEY_PASSWORD"],
    }
