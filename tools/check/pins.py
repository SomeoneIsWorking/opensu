"""Third-party sources openSU builds against, pinned by commit."""

from dataclasses import dataclass


@dataclass(frozen=True)
class GitPin:
    name: str
    url: str
    commit: str


LUCENT = GitPin(
    "lucent",
    "https://github.com/SomeoneIsWorking/lucent.git",
    "314c235cc9cdd7606fb06e71e99137324c6002d4",
)
GPU2D = GitPin(
    "gpu2d",
    "https://github.com/SomeoneIsWorking/gpu2d.git",
    "bf5174ce01abd103f47bb868a8167d68c0326caa",
)
ANDROID_PORT = GitPin(
    "android-port",
    "https://github.com/SomeoneIsWorking/android-port.git",
    "d85a6fa8bc350d8a40670d8ce8db5bb7c39c641e",
)
