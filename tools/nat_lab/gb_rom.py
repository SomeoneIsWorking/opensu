"""A Game Boy ROM for a core with no link: the screen is white, and black while A is held."""

from pathlib import Path

ROM_BYTES = 32 * 1024
ENTRY = 0x100
LOGO = 0x104
CODE = 0x150
TITLE = 0x134
HEADER_CHECK = 0x14D

ENTRY_JUMP = bytes.fromhex("00 C3 50 01")
# mGBA takes a ROM by the logo's first bytes.
LOGO_START = bytes.fromhex("CE ED 66 66")
# di; loop: select buttons; read them; A pressed? BGP = 0xFF, else 0x00; jr loop
PROGRAM = bytes.fromhex("F3 3E10 E000 F000 E601 2004 3EFF 1802 3E00 E047 18EC")


def build(out: Path) -> Path:
    image = bytearray(ROM_BYTES)
    image[ENTRY : ENTRY + len(ENTRY_JUMP)] = ENTRY_JUMP
    image[LOGO : LOGO + len(LOGO_START)] = LOGO_START
    image[TITLE : TITLE + 7] = b"ANYCORE"
    image[CODE : CODE + len(PROGRAM)] = PROGRAM
    check = 0
    for byte in image[TITLE:HEADER_CHECK]:
        check = (check - byte - 1) & 0xFF
    image[HEADER_CHECK] = check
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(bytes(image))
    return out
