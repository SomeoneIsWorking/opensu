"""Builds `link_test.gba`: Clang for the ARM7TDMI in Thumb, lld with `link.ld`, the raw image with
llvm-objcopy and the header's complement check written in. With `clock`, the image also reads and
draws a real-time clock, under a game code that makes mGBA give the cartridge one."""

import shutil
import subprocess
from pathlib import Path

SOURCE = Path(__file__).parent
HEADER_START = 0xA0
HEADER_END = 0xBD
GAME_CODE_AT = 0xAC
# Not on mGBA's list of known dumps, so `GBAOverrideApplyDefaults` takes it for a ROM hack of the
# game: a clock and 128 KiB of flash.
CLOCK_GAME_CODE = b"AXVE"


def _tool(name: str) -> str:
    found = shutil.which(name)
    if not found:
        raise RuntimeError(
            f"{name} is missing: `sudo dnf install clang lld llvm` (Fedora), "
            "`sudo apt install clang lld llvm` (Debian/Ubuntu)"
        )
    return found


def complement(image: bytes) -> int:
    """The cartridge header's complement check over 0xA0..0xBC."""
    return (-(sum(image[HEADER_START:HEADER_END]) + 0x19)) & 0xFF


def build(out: Path, clock: bool = False) -> Path:
    out.parent.mkdir(parents=True, exist_ok=True)
    work = out.parent / (out.stem + ".obj")
    work.mkdir(exist_ok=True)
    target = ["--target=armv4t-none-eabi", "-mcpu=arm7tdmi"]
    c_flags = ["-mthumb", "-ffreestanding", "-fno-builtin", "-O2", "-Wall", "-Werror"]
    if clock:
        c_flags.append("-DCLOCK_CARTRIDGE")
    clang = _tool("clang")
    subprocess.run([clang, *target, "-c", SOURCE / "crt0.s", "-o", work / "crt0.o"], check=True)
    subprocess.run(
        [clang, *target, *c_flags, "-c", SOURCE / "link_test.c", "-o", work / "link_test.o"],
        check=True,
    )
    elf = work / "link_test.elf"
    subprocess.run(
        [
            _tool("ld.lld"),
            "-T",
            SOURCE / "link.ld",
            work / "crt0.o",
            work / "link_test.o",
            "-o",
            elf,
        ],
        check=True,
    )
    raw = work / "link_test.bin"
    subprocess.run([_tool("llvm-objcopy"), "-O", "binary", elf, raw], check=True)
    image = bytearray(raw.read_bytes())
    if clock:
        image[GAME_CODE_AT : GAME_CODE_AT + len(CLOCK_GAME_CODE)] = CLOCK_GAME_CODE
    image[HEADER_END] = complement(image)
    out.write_bytes(bytes(image))
    return out
