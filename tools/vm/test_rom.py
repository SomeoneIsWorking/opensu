"""A generated NES test ROM (iNES, mapper 0) that draws a fixed checkerboard, for guest runs that
must start a real ROM without one from the player's library."""

from pathlib import Path

PRG_SIZE = 16 * 1024
CHR_SIZE = 8 * 1024
PRG_BASE = 0xC000
# Background, then the three colours of palette 0: green, red, blue, white.
PALETTE = bytes([0x2A, 0x16, 0x12, 0x30])


class _Assembler:
    """Just enough 6502 to place bytes and resolve labels for branches and absolute operands."""

    def __init__(self) -> None:
        self.code = bytearray()
        self.labels: dict[str, int] = {}
        self.fixups: list[tuple[int, str, str]] = []

    def here(self) -> int:
        return PRG_BASE + len(self.code)

    def label(self, name: str) -> None:
        self.labels[name] = self.here()

    def emit(self, *data: int) -> None:
        self.code += bytes(data)

    def branch(self, opcode: int, target: str) -> None:
        self.emit(opcode, 0)
        self.fixups.append((len(self.code) - 1, target, "relative"))

    def absolute(self, opcode: int, target: str) -> None:
        self.emit(opcode, 0, 0)
        self.fixups.append((len(self.code) - 2, target, "absolute"))

    def resolve(self) -> bytes:
        for offset, target, kind in self.fixups:
            address = self.labels[target]
            if kind == "relative":
                delta = address - (PRG_BASE + offset + 1)
                if not -128 <= delta <= 127:
                    raise ValueError(f"branch to {target} out of range")
                self.code[offset] = delta & 0xFF
            else:
                self.code[offset : offset + 2] = address.to_bytes(2, "little")
        return bytes(self.code)


def _program() -> tuple[bytes, dict[str, int]]:
    a = _Assembler()
    sta = 0x8D
    a.label("reset")
    a.emit(0x78, 0xD8)  # sei, cld
    a.emit(0xA2, 0x40, 0x8E, 0x17, 0x40)  # ldx #$40, stx $4017: no APU frame IRQ
    a.emit(0xA2, 0xFF, 0x9A, 0xE8)  # ldx #$ff, txs, inx
    a.emit(0x8E, 0x00, 0x20, 0x8E, 0x01, 0x20, 0x8E, 0x10, 0x40)  # PPU and DMC off
    for name in ("vblank1", "vblank2"):  # the PPU warms up over two frames
        a.label(name)
        a.emit(0x2C, 0x02, 0x20)  # bit $2002
        a.branch(0x10, name)  # bpl
    # Nametable 0 and its attributes cleared: every tile is tile 0.
    a.emit(0xA9, 0x20, sta, 0x06, 0x20, 0xA9, 0x00, sta, 0x06, 0x20)
    a.emit(0xA2, 0x04, 0xA0, 0x00)  # ldx #4, ldy #0
    a.label("clear")
    a.emit(sta, 0x07, 0x20, 0xC8)  # sta $2007, iny
    a.branch(0xD0, "clear")
    a.emit(0xCA)  # dex
    a.branch(0xD0, "clear")
    a.emit(0xA9, 0x3F, sta, 0x06, 0x20, 0xA9, 0x00, sta, 0x06, 0x20)  # $3F00: palettes
    a.emit(0xA2, 0x00)
    a.label("palette_loop")
    a.absolute(0xBD, "palette")  # lda palette,x
    a.emit(sta, 0x07, 0x20, 0xE8, 0xE0, len(PALETTE))  # sta $2007, inx, cpx
    a.branch(0xD0, "palette_loop")
    a.emit(0xA9, 0x00, sta, 0x05, 0x20, sta, 0x05, 0x20)  # scroll 0, 0
    a.emit(0xA9, 0x0A, sta, 0x01, 0x20)  # background on, left column included
    a.label("idle")
    a.absolute(0x4C, "idle")  # jmp idle
    a.label("interrupt")
    a.emit(0x40)  # rti
    a.label("palette")
    a.emit(*PALETTE)
    return a.resolve(), a.labels


def _chr() -> bytes:
    """Tile 0: four quadrants, one per colour of palette 0."""
    low = bytes([0x0F] * 4 + [0x00] * 4)
    high = bytes([0x00] * 4 + [0x0F] * 4)
    tile = low + high
    return tile + bytes(CHR_SIZE - len(tile))


def build() -> bytes:
    code, labels = _program()
    prg = bytearray(PRG_SIZE)
    prg[: len(code)] = code
    vectors = (labels["interrupt"], labels["reset"], labels["interrupt"])
    prg[-6:] = b"".join(address.to_bytes(2, "little") for address in vectors)
    header = b"NES\x1a" + bytes([1, 1, 0, 0]) + bytes(8)
    return header + bytes(prg) + _chr()


def write(path: Path) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(build())
    return path
