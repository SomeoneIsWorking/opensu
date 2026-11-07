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
    "37ff8c0bc15175c77311c82f73de5887abb0fa09",
)
GPU2D = GitPin(
    "gpu2d",
    "https://github.com/SomeoneIsWorking/gpu2d.git",
    "842bb6f2b49ea374b1da310c32b6765618122026",
)
ANDROID_PORT = GitPin(
    "android-port",
    "https://github.com/SomeoneIsWorking/android-port.git",
    "1dd7be0fc946b32e07b5283a6760d3a9733a3711",
)
