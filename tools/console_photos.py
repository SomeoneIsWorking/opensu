"""Builds the Photos console art theme: `uv run --frozen python -m tools.console_photos`.

Each card is a public-domain photo of the hardware on Wikimedia Commons, pinned by SHA-256 with its
page, author and licence. The originals go into scratch/console-photos/<system>/ and are only
read, as images, by ImageMagick: a white background is cut to alpha from the edges inwards, the
console is trimmed, fitted and centred on a square with a soft shadow, and the card is written as
assets/consoles/photos/cards/<system>.webp. The theme's README credits every photo.
"""

import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

from tools.download import fetch_verified
from tools.screenshots.encode import require_magick

REPO = Path(__file__).resolve().parents[1]
ORIGINALS = REPO / "scratch" / "console-photos"
THEME = REPO / "assets" / "consoles" / "photos"
COMMONS = "https://upload.wikimedia.org/wikipedia/commons"
PAGE = "https://commons.wikimedia.org/wiki/File:"
EVAN_AMOS = "Evan-Amos (Vanamo Online Game Museum)"
PUBLIC_DOMAIN = "Public domain"

# The console's longer side; the card is the console and its glow, in the console's own shape.
FIT = 448
# How far from white a background pixel may be, for photos shot on white without alpha.
BACKGROUND_FUZZ = "7%"
# A light glow behind every console, so a black one stands out on a dark gradient.
GLOW = "80x12+0+0"
QUALITY = 90


@dataclass(frozen=True)
class Photo:
    system: str
    path: str
    sha256: str
    author: str = EVAN_AMOS
    licence: str = PUBLIC_DOMAIN

    @property
    def file_name(self) -> str:
        return self.path.rsplit("/", 1)[-1]

    @property
    def url(self) -> str:
        return f"{COMMONS}/{self.path}"

    @property
    def page(self) -> str:
        return PAGE + self.file_name


PHOTOS = (
    Photo("nes", "b/b2/NES-Console-Set.png",
          "f8257bb9ae5fdd0f72955698014444d71ca4554810b0beab9748dff47b4651ea"),
    Photo("snes", "6/68/Nintendo-Super-NES-Console-FL.jpg",
          "01fc34b43aed0d3e0f0c548e1583f973f4629427050de243dffd25d3707797aa"),
    Photo("n64", "0/02/N64-Console-Set.png",
          "7fb67902fad4c9660d7fea4f4c18a0aedb0e33a832874eb1222b29e2034991ed"),
    Photo("gc", "8/8a/Nintendo-GameCube-Console-FL.jpg",
          "b96b43955f78b78f11390fe729efc2b5098860faec2408e63bb27f7a6ee32ee7"),
    Photo("wii", "f/f3/Wii-Console.png",
          "f1e0ae680f45c60580f9250b5a05e36bb78a24b97e4d6d7f576297c37cd5cc1d"),
    Photo("wiiu", "d/dd/Nintendo-Wii-U-Console-FL.jpg",
          "2002fe4abcbb20be0c673abdb9b9de259d0e192f92ed0d351195bab469514b8a"),
    Photo("switch", "7/76/Nintendo-Switch-Console-Docked-wJoyConRB.jpg",
          "489aa714bb0ffbdfeef9570d17799af28fa90120ed7ab5fb395a365f71e6b8b0"),
    Photo("gb", "f/f4/Game-Boy-FL.jpg",
          "10ca9840aa9a7315db9c288c004176debef5c819a03dfe07c5f0b62c890b4121"),
    Photo("gbc", "3/35/Nintendo-Game-Boy-Color-FL.png",
          "277de96b2ba9c9e4f8e958957dbe4c14556c3843e0f6302a0f912235f61f02fb"),
    Photo("gba", "0/0b/Nintendo-Game-Boy-Advance-Purple-FL.png",
          "bc88bac888992eff605a15ace75bea66a2d6a5b00fb2939d02e212534c60e326"),
    Photo("nds", "a/a3/Nintendo-DS-Fat-Blue.png",
          "38659907026f81c2fa97a1c0517a2aeb1546af63cfc06fa77aa08386a4f5f51e"),
    Photo("n3ds", "0/0a/Nintendo-3DS-AquaOpen.png",
          "2afabdaf2f4d0b31fef5ed5da0c22b5f896fbaf33dc0817d5ea51e3c22d67012"),
    Photo("psx", "9/95/PSX-Console-wController.png",
          "9a895e988235aac1d02a610ae0fe047a43e8c928e7a25f9bb388abcf94349c7c"),
    Photo("ps2", "0/02/PS2-Fat-Console-Set.jpg",
          "937b04a3244723adc052cf444471f8aefaf3bead7bdf24a2b8d785e002e90c3e"),
    Photo("ps3", "9/9d/Sony-PlayStation-3-CECHA01-Console-FL.jpg",
          "c05b11f333a899cc5343b20b726e4f6ab32fc974ab950e6485264184a902650b"),
    Photo("ps4", "4/4a/Sony-PlayStation-4-PS4-Console-FL.png",
          "9b0b6661dfdd52bcbf8cdf4175d914850a6f3eb0d97a01b237499adaa1801f97"),
    Photo("psp", "5/5d/Sony-PSP-1000-Body.png",
          "ff7396277e084338cf9dabb55b673d8fd338aea21e0ac2818f05f5887fca23cb"),
    Photo("dreamcast", "a/a5/Sega-Dreamcast-Console-FL.png",
          "18ab45afb5e8012f8906c69b3afa8c498c04151e6ad6201023ba219d8dfd5db9"),
    Photo("xbox", "c/c2/Xbox-Console-Set.png",
          "cb2f1716a69cc250a1d139ad97db6da0746d5c4f41336d693b92ba0b104477ff"),
    Photo("xbox360", "a/ad/Microsoft-Xbox-360-Pro-Console-FL.png",
          "ec78fc13f296155a7c6e011727ab03247042d742f7d6e39727c7c6bcb0b03ad2"),
    # No public-domain cabinet stands alone on a plain background; an arcade stick does.
    Photo("arcade", "0/0a/Sega-Dreamcast-Arcade-Stick.jpg",
          "a8180a202583a7fb76a845b574f2653163fa2da9a1af27e4c5051235fcb008a0"),
)  # fmt: skip


def card(magick: str, original: Path, out: Path) -> None:
    """One card: the background cut to alpha, the console trimmed, fitted and lit from behind."""
    out.parent.mkdir(parents=True, exist_ok=True)
    opaque = (
        subprocess.run(
            [magick, "identify", "-format", "%[opaque]", str(original)],
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
        == "True"
    )
    command = [magick, str(original), "-auto-orient", "-colorspace", "sRGB", "-alpha", "set"]
    if opaque:
        # A photo on white: the white reached from the edges goes, a white console keeps its body.
        command += ["-bordercolor", "white", "-border", "1", "-fuzz", BACKGROUND_FUZZ,
                    "-fill", "none", "-draw", "color 0,0 floodfill", "-shave", "1x1",
                    "-fuzz", "0"]  # fmt: skip
    command += ["-trim", "+repage", "-resize", f"{FIT}x{FIT}"]
    command += ["(", "+clone", "-background", "white", "-shadow", GLOW, ")", "+swap",
                "-background", "none", "-layers", "merge", "+repage", "-strip"]  # fmt: skip
    command += ["-define", "webp:alpha-quality=100", "-quality", str(QUALITY), str(out)]
    subprocess.run(command, check=True)


def credits() -> str:
    rows = "\n".join(
        f"| `{p.system}` | [{p.file_name}]({p.page}) | {p.author} | {p.licence} |" for p in PHOTOS
    )
    return (
        "# Photos\n\n"
        "Public-domain photos of the hardware from Wikimedia Commons, made into cards by\n"
        "`tools/console_photos.py` (background cut to alpha, trimmed, a soft light glow behind).\n"
        "The glyphs are the Drawings theme's. A system without a photo here has no picture on its\n"
        "card in this theme.\n\n"
        "| System | Commons page | Author | Licence |\n| --- | --- | --- | --- |\n"
        f"{rows}\n"
    )


def main() -> int:
    magick = require_magick()
    only = set(sys.argv[1:])
    for photo in PHOTOS:
        if only and photo.system not in only:
            continue
        original = fetch_verified(
            photo.url, photo.sha256, ORIGINALS / photo.system / photo.file_name
        )
        out = THEME / "cards" / f"{photo.system}.webp"
        card(magick, original, out)
        print(f"{photo.system}: {out}")
    (THEME / "README.md").write_text(credits())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
