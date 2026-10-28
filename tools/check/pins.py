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
    "19f18cc295a45406d12f0d0c29b06f18aeea5201",
)
GPU2D = GitPin(
    "gpu2d",
    "https://github.com/SomeoneIsWorking/gpu2d.git",
    "a6c7ab21ab9e5767f152abc9f18fb915125c6e9c",
)
