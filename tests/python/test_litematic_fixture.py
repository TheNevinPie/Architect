"""M1 COMPLETED: .litematic fixture conformance (format-level specification).

Validates fixtures/small.litematic against fixtures/small.expected.json
with an independent pure-Python NBT reader: structure, palette, packed
cells (incl. the word-straddling index 21), tile entities, and the
readLitematic validation rules (Version range, array length, id range).

This file is the conformance target for the M2 C++ decoder: the C++
readLitematic MUST accept/reject exactly these cases.

Run: python tests/python/test_litematic_fixture.py
"""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from nbt_min import (  # noqa: E402
    TAG_COMPOUND, TAG_INT, TAG_LIST, TAG_LONG, TAG_LONG_ARRAY, TAG_STRING,
    NbtError, c_get, read_root_gzip,
)
from litematic_bitpack import (  # noqa: E402
    bits_per_entry, flat_index_litematic, packed_long_count, unpack_entry,
)

FAILURES = []


def check(name, cond, detail=""):
    print(("PASS " if cond else "FAIL ") + name + (" | " + str(detail) if detail and not cond else ""))
    if not cond:
        FAILURES.append(name)


HERE = os.path.dirname(os.path.abspath(__file__))
FIX = os.path.join(HERE, "..", "..", "fixtures")
with open(os.path.join(FIX, "small.litematic"), "rb") as f:
    FILE_BYTES = f.read()
with open(os.path.join(FIX, "small.expected.json"), encoding="utf-8") as f:
    EXP = json.load(f)

SX, SY, SZ = EXP["size"]
VOLUME = EXP["volume"]

# --- root structure ---
_, root = read_root_gzip(FILE_BYTES)
version = c_get(root, "Version", TAG_INT)
check("Version in 2..7", 2 <= version <= 7, version)
check("Version == 7", version == EXP["version"])
check("SubVersion round-trips", c_get(root, "SubVersion", TAG_INT) == EXP["subVersion"])
check("DataVersion round-trips (opaque)", c_get(root, "MinecraftDataVersion", TAG_INT) == EXP["dataVersion"])
meta = c_get(root, "Metadata", TAG_COMPOUND)
enc = c_get(meta, "EnclosingSize", TAG_COMPOUND)
check("EnclosingSize matches region",
      (c_get(enc, "x", TAG_INT), c_get(enc, "y", TAG_INT), c_get(enc, "z", TAG_INT)) == tuple(EXP["enclosingSize"]))
check("RegionCount == 2", c_get(meta, "RegionCount", TAG_INT) == EXP["regionCount"] == 2)
check("TotalBlocks == non-air count", c_get(meta, "TotalBlocks", TAG_INT) == EXP["totalBlocksAll"])
check("TotalVolume == volume", c_get(meta, "TotalVolume", TAG_INT) == EXP["totalVolume"])
regions = c_get(root, "Regions", TAG_COMPOUND)
check("region 'test' present", EXP["region"] in regions)
check("region 'upper' present", EXP["region2"]["name"] in regions)
_, reg = regions[EXP["region"]]
check("region is compound", _ == TAG_COMPOUND)

# --- region ---
pos = c_get(reg, "Position", TAG_COMPOUND)
check("Position round-trips",
      (c_get(pos, "x", TAG_INT), c_get(pos, "y", TAG_INT), c_get(pos, "z", TAG_INT)) == tuple(EXP["position"]))
size = c_get(reg, "Size", TAG_COMPOUND)
sx, sy, sz = (c_get(size, "x", TAG_INT), c_get(size, "y", TAG_INT), c_get(size, "z", TAG_INT))
check("Size round-trips", (sx, sy, sz) == (SX, SY, SZ))
check("Size abs() normalization", (abs(sx), abs(sy), abs(sz)) == (SX, SY, SZ))

palette = c_get(reg, "BlockStatePalette", TAG_LIST)
ptype, pentries = palette
check("palette is compound list", ptype == TAG_COMPOUND)
check("palette length 7", len(pentries) == 7, len(pentries))
for i, entry in enumerate(pentries):
    want = EXP["palette"][i]
    check("palette[%d] name" % i, c_get(entry, "Name", TAG_STRING) == want["name"])
    if want["properties"]:
        _, props = entry["Properties"]
        got = {k: v for k, (t, v) in props.items()}
        check("palette[%d] props" % i, got == want["properties"], got)
check("palette[0] is air", c_get(pentries[0], "Name", TAG_STRING) == "minecraft:air")

# --- packed cells ---
words = c_get(reg, "BlockStates", TAG_LONG_ARRAY)
bits = bits_per_entry(len(pentries))
check("bits == 3", bits == EXP["bits"] == 3)
check("BlockStates length == ceil(v*b/64)",
      len(words) == packed_long_count(VOLUME, bits) == 2, len(words))
check("words match generator", words == EXP["words"])

cells_ok = True
for flat, want in EXP["cells"].items():
    if unpack_entry(words, int(flat), bits, len(pentries)) != want:
        cells_ok = False
        break
check("all 24 cells decode to expected ids", cells_ok)
check("straddle cell idx21 == oak_log(4)", unpack_entry(words, 21, bits, 7) == 4)
check("last cell idx23 == wall_torch(5)", unpack_entry(words, 23, bits, 7) == 5)
check("known stone idx0", unpack_entry(words, 0, bits, 7) == 1)

# spot-check flat-index math against coordinates, not the generator table
check("coord (0,1,3) is flat 21", flat_index_litematic(0, 1, 3, SX, SZ) == 21)
check("coord (2,1,3) is flat 23", flat_index_litematic(2, 1, 3, SX, SZ) == 23)

# --- tile entities / ticks ---
tents = c_get(reg, "TileEntities", TAG_LIST)
check("one tile entity", len(tents[1]) == 1)
tent = tents[1][0]
check("tile entity id", c_get(tent, "id", TAG_STRING) == "minecraft:chest")
check("tile entity pos",
      (c_get(tent, "x", TAG_INT), c_get(tent, "y", TAG_INT), c_get(tent, "z", TAG_INT)) == (0, 0, 3))
for key in ("Entities", "PendingBlockTicks", "PendingFluidTicks"):
    check("%s present" % key, key in reg)

# --- second region: independent palette/position/size ---
R2 = EXP["region2"]
_, reg2 = regions[R2["name"]]
pos2 = c_get(reg2, "Position", TAG_COMPOUND)
check("upper Position",
      (c_get(pos2, "x", TAG_INT), c_get(pos2, "y", TAG_INT), c_get(pos2, "z", TAG_INT)) == tuple(R2["position"]))
size2 = c_get(reg2, "Size", TAG_COMPOUND)
check("upper Size",
      (c_get(size2, "x", TAG_INT), c_get(size2, "y", TAG_INT), c_get(size2, "z", TAG_INT)) == tuple(R2["size"]))
pal2 = c_get(reg2, "BlockStatePalette", TAG_LIST)
check("upper palette length 4", len(pal2[1]) == 4, len(pal2[1]))
check("upper palette[1] brick_stairs",
      c_get(pal2[1][1], "Name", TAG_STRING) == "minecraft:brick_stairs")
_, props2 = pal2[1][1]["Properties"]
check("upper stairs props",
      {k: v for k, (t, v) in props2.items()} == R2["palette"][1]["properties"])
words2 = c_get(reg2, "BlockStates", TAG_LONG_ARRAY)
bits2 = bits_per_entry(len(pal2[1]))
check("upper bits == 2", bits2 == R2["bits"] == 2)
check("upper 1 word", len(words2) == packed_long_count(R2["volume"], bits2) == 1)
cells2_ok = all(unpack_entry(words2, int(f), bits2, len(pal2[1])) == w
                for f, w in R2["cells"].items())
check("upper all 4 cells decode", cells2_ok)
check("upper furnace cell idx1", unpack_entry(words2, 1, bits2, 4) == 3)
tents2 = c_get(reg2, "TileEntities", TAG_LIST)
check("upper one tile entity", len(tents2[1]) == 1)
tent2 = tents2[1][0]
check("upper tile entity furnace",
      c_get(tent2, "id", TAG_STRING) == "minecraft:furnace"
      and (c_get(tent2, "x", TAG_INT), c_get(tent2, "y", TAG_INT), c_get(tent2, "z", TAG_INT)) == (1, 0, 0))


# --- validation rules (what the M2 C++ readLitematic must enforce) ---
def validate(version, palette_len, volume, words_len, max_id_ok=True):
    """Mirror of the readLitematic contract in schematic/Readers.h."""
    if version == 1 or version > 7:
        return "reject: Version %d" % version
    if palette_len == 0:
        return "reject: empty palette"
    b = bits_per_entry(palette_len)
    if words_len != packed_long_count(volume, b):
        return "reject: BlockStates length"
    if not max_id_ok:
        return "reject: palette id out of range"
    return "accept"


check("rule: Version 1 rejected", validate(1, 7, VOLUME, 2).startswith("reject"))
check("rule: Version 8 rejected", validate(8, 7, VOLUME, 2).startswith("reject"))
check("rule: Version 7 accepted", validate(7, 7, VOLUME, 2) == "accept")
check("rule: short array rejected", validate(7, 7, VOLUME, 1).startswith("reject"))
check("rule: empty palette rejected", validate(7, 0, VOLUME, 0).startswith("reject"))
check("rule: bad id rejected", validate(7, 7, VOLUME, 2, False).startswith("reject"))

# --- corrupt bytes are hard errors, never silent ---
try:
    read_root_gzip(FILE_BYTES[:40])
    check("truncated file raises", False)
except Exception:
    check("truncated file raises", True)
try:
    read_root_gzip(b"not-gzip-at-all................................")
    check("non-gzip raises", False)
except Exception:
    check("non-gzip raises", True)

print("%d failure(s)" % len(FAILURES))
sys.exit(1 if FAILURES else 0)
