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
    "1a212bc4e145174090f5f7e40202d8f7c4eb1292",
)
GPU2D = GitPin(
    "gpu2d",
    "https://github.com/SomeoneIsWorking/gpu2d.git",
    "ee86d8a7c03c84c22ea8d730173a5057a879e663",
)
