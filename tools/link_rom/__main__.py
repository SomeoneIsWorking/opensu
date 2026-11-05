import argparse
from pathlib import Path

from tools.checkout import REPO
from tools.link_rom.build import build

parser = argparse.ArgumentParser(prog="tools.link_rom", description=__doc__)
parser.add_argument("--out", type=Path)
parser.add_argument("--clock", action="store_true", help="a cartridge with a real-time clock")
args = parser.parse_args()
name = "link_clock.gba" if args.clock else "link_test.gba"
print(build(args.out or REPO / "scratch" / "link-rom" / name, args.clock))
