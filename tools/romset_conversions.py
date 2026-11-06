#!/usr/bin/env python3
"""Generate assets/romset_conversions.json and assets/cps2_keys.json.

Reads listings only: names, sizes and CRCs. A layout is written when CRC arithmetic proves it, or
for CPS3's fixed SIMM layout. openSU checks every rebuilt ROM by CRC against the target listing.
"""

import argparse
import collections
import glob
import itertools
import json
import os
import re
import xml.etree.ElementTree as ET
import zlib

KEY_SIZE = 20
DEAD_BOARD_KEY = 0x2CF772B0
CPS3_SIMM = 0x200000
CPS3_FILE = 0x800000
MAX_JOIN = 8


class Crc:
    """crc32 of data built from parts, without the data."""

    def __init__(self):
        self.operators = {}
        self.fills = {}

    @staticmethod
    def times(matrix, vector):
        total = 0
        row = 0
        while vector:
            if vector & 1:
                total ^= matrix[row]
            vector >>= 1
            row += 1
        return total

    def square(self, matrix):
        return [self.times(matrix, x) for x in matrix]

    def operator(self, length):
        if length not in self.operators:
            power = self.square(
                self.square(self.square([0xEDB88320] + [1 << i for i in range(31)]))
            )
            result = None
            remaining = length
            while remaining:
                if remaining & 1:
                    result = power if result is None else [self.times(power, x) for x in result]
                remaining >>= 1
                power = self.square(power)
            self.operators[length] = result
        return self.operators[length]

    def join(self, first, second, second_length):
        return self.times(self.operator(second_length), first) ^ second

    def repeat(self, crc, length, count):
        result = None
        while count:
            if count & 1:
                result = crc if result is None else self.join(result, crc, length)
            count >>= 1
            if count:
                crc = self.join(crc, crc, length)
                length *= 2
        return result

    def fill(self, byte, length):
        if (byte, length) not in self.fills:
            self.fills[(byte, length)] = zlib.crc32(bytes([byte]) * length)
        return self.fills[(byte, length)]


Rom = collections.namedtuple("Rom", "name size crc merge")


class Game:
    def __init__(self, name, parent, romof, source, roms):
        self.name = name
        self.parent = parent
        self.romof = romof
        self.source = source
        self.roms = roms


def read_listing(paths):
    games = {}
    for path in paths:
        for _, element in ET.iterparse(path):
            if element.tag not in ("game", "machine"):
                continue
            roms = []
            for rom in element.findall("rom"):
                if rom.get("status") != "nodump" and rom.get("crc"):
                    roms.append(
                        Rom(
                            rom.get("name").lower(),
                            int(rom.get("size") or 0),
                            int(rom.get("crc"), 16),
                            rom.get("merge"),
                        )
                    )
            if element.get("isdevice") != "yes":
                games[element.get("name")] = Game(
                    element.get("name"),
                    element.get("cloneof"),
                    element.get("romof"),
                    element.get("sourcefile") or "",
                    roms,
                )
            element.clear()
    return games


def optional_roms(source_root):
    """(name, crc) pairs that every driver table marks BRF_OPT."""
    row = re.compile(
        r'\{\s*"([^"]+)"\s*,\s*(?:0x[0-9a-fA-F]+|\d+)\s*,\s*(0x[0-9a-fA-F]+|0)\s*,\s*([^}]*)\}'
    )
    seen = collections.defaultdict(set)
    for path in glob.glob(os.path.join(source_root, "src/burn/**/*.cpp"), recursive=True):
        with open(path, errors="replace") as source:
            text = source.read()
        if "BurnRomInfo" in text:
            for name, crc, flags in row.findall(text):
                seen[(name.lower(), int(crc, 16))].add("BRF_OPT" in flags)
    return {key for key, flags in seen.items() if flags == {True}}


def key_bytes(words):
    data = bytearray(KEY_SIZE)
    for b in range(KEY_SIZE * 8):
        if words[b // 16] & (0x8000 >> (b % 16)):
            bit = (317 - b) % (KEY_SIZE * 8)
            data[bit // 8] |= 1 << ((bit ^ 7) % 8)
    return bytes(data)


def proven_keys(table_path, wanted):
    """Keys of the FB Alpha table whose rebuilt file hashes to a CRC FBNeo lists, by that CRC.

    A table name can mean another driver here, so names are never used.
    """
    with open(table_path, errors="replace") as source:
        text = source.read()
    keys = {DEAD_BOARD_KEY: [0xFFFF] * 10}
    assert zlib.crc32(b"\xff" * KEY_SIZE) == DEAD_BOARD_KEY
    row = re.compile(
        r'\{\s*"\w+",\s*\{\s*0x([0-9a-f]+),\s*0x([0-9a-f]+)\s*\},\s*(?:0x[0-9a-f]+|0)\s*\},?\s*//\s*([^\n]*)'
    )
    for first, second, comment in row.findall(text):
        watchdog = [int(w, 16) for w in re.findall(r"\b[0-9A-Fa-f]{4}\b", comment.split("  ")[0])]
        if len(watchdog) != 3:
            continue
        first = int(first, 16)
        second = int(second, 16)
        head = [
            first >> 16,
            first & 0xFFFF,
            second >> 16,
            second & 0xFFFF,
            watchdog[2],
            watchdog[1],
            watchdog[0],
            0x4000,
            0x0900,
        ]
        for high, low in itertools.product((0, 0xFC00), range(1024)):
            words = head + [high | low]
            crc = zlib.crc32(key_bytes(words))
            if crc in wanted:
                keys[crc] = words
                break
    return keys


Piece = collections.namedtuple("Piece", "source offset stride length fill")


def whole(rom, length=None):
    return Piece(rom.crc, 0, 1, length or rom.size, 0)


class Layouts:
    """Finds how spare old files make up the ROMs a driver misses."""

    def __init__(self, crc):
        self.crc = crc

    def single(self, rom, old):
        crc = self.crc
        if old.size > rom.size:
            if (
                old.size % rom.size == 0
                and crc.repeat(rom.crc, rom.size, old.size // rom.size) == old.crc
            ):
                return [whole(old, rom.size)]
            pad = old.size - rom.size
            if any(crc.join(rom.crc, crc.fill(b, pad), pad) == old.crc for b in (0, 255)):
                return [whole(old, rom.size)]
        elif rom.size > old.size:
            if (
                rom.size % old.size == 0
                and crc.repeat(old.crc, old.size, rom.size // old.size) == rom.crc
            ):
                return [whole(old)] * (rom.size // old.size)
            pad = rom.size - old.size
            for byte in (0, 255):
                if crc.join(old.crc, crc.fill(byte, pad), pad) == rom.crc:
                    return [whole(old), Piece(0, 0, 1, pad, byte)]
        return None

    def ordered_parts(self, target, parts):
        if len(parts) > 12:
            return None
        for count in range(2, min(len(parts), MAX_JOIN) + 1):
            groups = (
                itertools.permutations(parts, count)
                if count <= 3
                else itertools.combinations(parts, count)
            )
            for group in groups:
                if sum(p.size for p in group) != target.size:
                    continue
                crc = group[0].crc
                for part in group[1:]:
                    crc = self.crc.join(crc, part.crc, part.size)
                if crc == target.crc:
                    return group
        return None

    def solve(self, missing, spare):
        recipes = {}
        used = set()
        for rom in missing:
            for old in spare:
                pieces = None if old.crc in used else self.single(rom, old)
                if pieces:
                    recipes[rom.crc] = pieces
                    used.add(old.crc)
                    break
        for rom in missing:
            if rom.crc not in recipes:
                group = self.ordered_parts(
                    rom, [o for o in spare if o.crc not in used and o.size < rom.size]
                )
                if group:
                    recipes[rom.crc] = [whole(p) for p in group]
                    used.update(p.crc for p in group)
        for old in spare:
            if old.crc not in used:
                group = self.ordered_parts(
                    old, [r for r in missing if r.crc not in recipes and r.size < old.size]
                )
                if group:
                    offset = 0
                    for part in group:
                        recipes[part.crc] = [Piece(old.crc, offset, 1, part.size, 0)]
                        offset += part.size
                    used.add(old.crc)
        return recipes, used


def cps3_recipes(missing, spare):
    """An old CPS3 file is four SIMMs: bytes 0-3 of 4 for program, else two interleaved pairs."""
    files = sorted((o for o in spare if o.size == CPS3_FILE), key=lambda o: (len(o.name), o.name))
    simms = [r for r in missing if r.size == CPS3_SIMM]
    if not files or len(simms) != 4 * len(files) or len(simms) != len(missing):
        return None
    recipes = {}
    for index, old in enumerate(files):
        group = simms[4 * index : 4 * index + 4]
        program = "simm1" in group[0].name or "simm2" in group[0].name
        for lane, rom in enumerate(group):
            if program:
                recipes[rom.crc] = [Piece(old.crc, lane, 4, CPS3_SIMM, 0)]
            else:
                recipes[rom.crc] = [
                    Piece(old.crc, (lane // 2) * (CPS3_FILE // 2) + lane % 2, 2, CPS3_SIMM, 0)
                ]
    return recipes


class Generator:
    def __init__(self, fbneo, optional, keys):
        self.fbneo = fbneo
        self.optional = optional
        self.keys = keys
        self.layouts = Layouts(Crc())
        self.recipes = {}
        self.aliases = set()
        self.accepted = set()
        self.optional_needed = set()
        self.by_crc = collections.defaultdict(set)
        self.by_name = collections.defaultdict(set)
        for game in fbneo.values():
            for rom in game.roms:
                if not rom.merge:
                    self.by_crc[rom.crc].add(game.name)
                    self.by_name[rom.name].add(game.name)

    def required(self, game):
        return [r for r in game.roms if (r.name, r.crc) not in self.optional]

    @staticmethod
    def pool(listing, game):
        roms = []
        seen = set()
        while game is not None and game.name not in seen:
            seen.add(game.name)
            roms += game.roms
            game = listing.get(game.romof) if game.romof else None
        return roms

    def fit(self, listing, game, driver):
        """The recipes that make the old set run this driver, or None."""
        pool = self.pool(listing, game)
        crcs = {r.crc for r in pool}
        required = self.required(driver)
        keys = [
            r
            for r in required
            if r.name.endswith(".key") and r.size == KEY_SIZE and r.crc not in crcs
        ]
        if any(k.crc not in self.keys for k in keys):
            return None
        missing = [r for r in required if r.crc not in crcs and r not in keys]
        wanted = {r.crc for r in required}
        spare = list({(r.crc, r.size): r for r in pool if r.crc not in wanted}.values())
        recipes, used = self.layouts.solve(missing, spare) if missing else ({}, set())
        rest = [r for r in missing if r.crc not in recipes]
        if rest and "cps3" in driver.source:
            extra = cps3_recipes(rest, spare)
            if extra:
                recipes.update(extra)
                used.update(p.source for pieces in extra.values() for p in pieces)
                rest = []
        if rest:
            return None
        absent = {
            r.crc
            for r in driver.roms
            if r.crc not in crcs and r.crc not in recipes and r not in keys
        }
        return recipes, absent, used

    def candidates(self, game):
        own = [r for r in game.roms if not r.merge] or game.roms
        by_crc = collections.Counter(n for r in own for n in self.by_crc.get(r.crc, ()))
        by_name = collections.Counter(n for r in own for n in self.by_name.get(r.name, ()))
        names = [n for n, _ in by_crc.most_common(3)]
        names += [n for n, c in by_name.most_common(2) if c * 2 >= len(own)]
        if game.name in self.fbneo:
            names.insert(0, game.name)
        return list(dict.fromkeys(names))

    def add(self, listing):
        for game in listing.values():
            if not game.roms:
                continue
            best = None
            for name in self.candidates(game):
                driver = self.fbneo[name]
                fit = self.fit(listing, game, driver)
                if fit is None:
                    continue
                if name != game.name:
                    # a renamed driver must consume the set's own ROMs, or it is another revision
                    known = {r.crc for r in driver.roms} | fit[2]
                    if any(
                        r.crc not in known
                        for r in ([r for r in game.roms if not r.merge] or game.roms)
                    ):
                        continue
                rank = name != game.name
                if best is None or rank < best[0]:
                    best = (rank, name, fit)
            if best is None:
                continue
            _, name, (recipes, absent, _) = best
            self.recipes.update(recipes)
            self.optional_needed |= absent
            if name != game.name:
                self.aliases.add((game.name, name))
            self.accepted.add(game.name)

    def write_conversions(self, path):
        aliases = collections.defaultdict(list)
        for zip_name, driver in sorted(self.aliases):
            aliases[zip_name].append(driver)
        recipes = [
            {"crc": crc, "pieces": [list(p) for p in self.recipes[crc]]}
            for crc in sorted(self.recipes)
        ]
        with open(path, "w") as out:
            json.dump(
                {"recipes": recipes, "aliases": aliases, "optional": sorted(self.optional_needed)},
                out,
                separators=(",", ":"),
            )
            out.write("\n")

    def write_keys(self, path):
        rows = [{"crc": crc, "words": self.keys[crc]} for crc in sorted(self.keys)]
        with open(path, "w") as out:
            json.dump({"keys": rows}, out, separators=(",", ":"))
            out.write("\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fbneo-source", required=True, help="FBNeo checkout, for BRF_OPT")
    parser.add_argument("--assets", required=True, help="where the two files go")
    parser.add_argument("--fbneo", nargs="+", required=True, help="FBNeo's own datafiles")
    parser.add_argument("--listing", nargs="+", required=True, help="old romset listings")
    parser.add_argument("--key-table", required=True, help="a cps2_crpt source with keys_table")
    args = parser.parse_args()
    crc = Crc()
    assert crc.join(zlib.crc32(b"abc"), zlib.crc32(b"defg"), 4) == zlib.crc32(b"abcdefg")
    assert crc.repeat(zlib.crc32(b"xy"), 2, 3) == zlib.crc32(b"xyxyxy")
    fbneo = read_listing(args.fbneo)
    wanted = {
        r.crc
        for g in fbneo.values()
        for r in g.roms
        if r.name.endswith(".key") and r.size == KEY_SIZE
    }
    keys = proven_keys(args.key_table, wanted)
    generator = Generator(fbneo, optional_roms(args.fbneo_source), keys)
    for path in args.listing:
        generator.add(read_listing([path]))
    generator.write_conversions(os.path.join(args.assets, "romset_conversions.json"))
    generator.write_keys(os.path.join(args.assets, "cps2_keys.json"))
    print(
        f"recipes {len(generator.recipes)} keys {len(keys)} of {len(wanted)} "
        f"aliases {len(generator.aliases)} optional {len(generator.optional_needed)} "
        f"sets {len(generator.accepted)}"
    )


if __name__ == "__main__":
    main()
