import argparse
from pathlib import Path

from tools.checkout import REPO
from tools.link_rom.build import build

parser = argparse.ArgumentParser(prog="tools.link_rom", description=__doc__)
parser.add_argument("--out", type=Path, default=REPO / "scratch" / "link-rom" / "link_test.gba")
print(build(parser.parse_args().out))
