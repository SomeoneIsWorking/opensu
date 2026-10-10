"""The host's SteamOS-style session setup, read only, ready to write into the guest."""

import re
from dataclasses import dataclass

from tools.vm import config


@dataclass(frozen=True)
class SessionSetup:
    picker: bytes
    select: bytes
    picker_entry: bytes
    sddm_conf: bytes


def retarget_autologin_user(conf: str, user: str) -> str:
    """The host's SDDM config with its autologin user replaced by the guest's."""
    return re.sub(r"(?m)^User=.*$", f"User={user}", conf)


def read_host() -> SessionSetup:
    conf = config.HOST_SDDM_CONF.read_text()
    return SessionSetup(
        picker=config.HOST_PICKER.read_bytes(),
        select=config.HOST_SELECT.read_bytes(),
        picker_entry=config.HOST_PICKER_ENTRY.read_bytes(),
        sddm_conf=retarget_autologin_user(conf, config.TEST_USER).encode(),
    )
