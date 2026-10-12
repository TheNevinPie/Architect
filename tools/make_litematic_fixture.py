"""Generates fixtures/small.litematic + fixtures/small.expected.json.

Region "test": pos (10,64,-5), size (3,2,4) — non-cubic to catch
axis swaps. Palette of 7 forces 3 bits/entry over 24 cells = 2 words,
so flat index 21 straddles the word boundary (pins straddle decoding).

Region "upper": pos (20,70,10), size (2,1,2), own 4-entry palette
(2 bits/entry, 1 word) + furnace tile entity — pins multi-region
decode with independent palettes and positions.

Layout (flat = y*12 + z*3 + x):
  idx  0 (0,0,0) stone
  idx  2 (2,0,0) glass
  idx  4 (1,0,1) oak_stairs
  idx  9 (0,0,3) chest (+ TileEntity)
  idx 19 (1,1,2) stone
  idx 21 (0,1,3) oak_log      <- straddles words 0/1
  idx 23 (2,1,3) wall_torch
  all others air.

Tag names follow litemapy/schematic.py (master): BlockStatePalette,
BlockStates, Position, Size, Entities, TileEntities, PendingBlockTicks,
PendingFluidTicks, Version/SubVersion/MinecraftDataVersion/Metadata.
"""

import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tests", "python"))

from nbt_min import (  # noqa: E402
    TAG_BYTE, TAG_COMPOUND, TAG_INT, TAG_LIST, TAG_LONG, TAG_LONG_ARRAY,
    TAG_STRING, write_root_gzip,
)
from litematic_bitpack import (  # noqa: E402
    bits_per_entry, flat_index_litematic, pack_entries, packed_long_count,
    to_signed,
)

SX, SY, SZ = 3, 2, 4
VOLUME = SX * SY * SZ

PALETTE = [
    {"Name": "minecraft:air", "Properties": None},
    {"Name": "minecraft:stone", "Properties": None},
    {"Name": "minecraft:oak_stairs", "Properties": {
        "facing": "east", "half": "bottom", "shape": "straight", "waterlogged": "false"}},
    {"Name": "minecraft:glass", "Properties": None},
    {"Name": "minecraft:oak_log", "Properties": {"axis": "y"}},
    {"Name": "minecraft:wall_torch", "Properties": {"facing": "north"}},
    {"Name": "minecraft:chest", "Properties": {
        "facing": "south", "type": "single", "waterlogged": "false"}},
]

# (x, y, z) -> palette id
PLACED = {
    (0, 0, 0): 1,
    (2, 0, 0): 3,
    (1, 0, 1): 2,
    (0, 0, 3): 6,
    (1, 1, 2): 1,
    (0, 1, 3): 4,
    (2, 1, 3): 5,
}

REGION_POS = (10, 64, -5)
REGION2_POS = (20, 70, 10)
REGION2_SIZE = (2, 1, 2)
DATA_VERSION = 4189  # example value; opaque to the reader (round-trip only)


def palette_entry_nbt(entry):
    comp = {"Name": (TAG_STRING, entry["Name"])}
    if entry["Properties"]:
        props = {k: (TAG_STRING, v) for k, v in entry["Properties"].items()}
        comp["Properties"] = (TAG_COMPOUND, props)
    return (TAG_COMPOUND, comp)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    fixtures = os.path.join(here, "..", "fixtures")
    os.makedirs(fixtures, exist_ok=True)

    bits = bits_per_entry(len(PALETTE))
    assert bits == 3, bits
    ids = []
    for y in range(SY):
        for z in range(SZ):
            for x in range(SX):
                assert flat_index_litematic(x, y, z, SX, SZ) == len(ids)
                ids.append(PLACED.get((x, y, z), 0))
    assert len(ids) == VOLUME
    assert packed_long_count(VOLUME, bits) == 2
    words = to_signed(pack_entries(ids, bits))

    def build_region(pos, size, palette_entries, cell_ids, tile_ents):
        palette_tags = []
        for _, tag in [palette_entry_nbt(e) for e in palette_entries]:
            palette_tags.append(tag)
        reg = {
            "Position": (TAG_COMPOUND, {
                "x": (TAG_INT, pos[0]), "y": (TAG_INT, pos[1]), "z": (TAG_INT, pos[2])}),
            "Size": (TAG_COMPOUND, {
                "x": (TAG_INT, size[0]), "y": (TAG_INT, size[1]), "z": (TAG_INT, size[2])}),
            "BlockStatePalette": (TAG_LIST, (TAG_COMPOUND, palette_tags)),
            "Entities": (TAG_LIST, (TAG_COMPOUND, [])),
            "PendingBlockTicks": (TAG_LIST, (TAG_COMPOUND, [])),
            "PendingFluidTicks": (TAG_LIST, (TAG_COMPOUND, [])),
        }
        return reg

    region = build_region(REGION_POS, (SX, SY, SZ), PALETTE, ids, None)
    region["BlockStates"] = (TAG_LONG_ARRAY, words)
    region["TileEntities"] = (TAG_LIST, (TAG_COMPOUND, [{
        "id": (TAG_STRING, "minecraft:chest"),
        "x": (TAG_INT, 0), "y": (TAG_INT, 0), "z": (TAG_INT, 3),
    }]))
    non_air = sum(1 for i in ids if i != 0)

    # --- second region "upper": 2x1x2 at REGION2_POS, own palette/states ---
    PALETTE2 = [
        {"Name": "minecraft:air", "Properties": None},
        {"Name": "minecraft:brick_stairs", "Properties": {
            "facing": "south", "half": "top", "shape": "inner_left", "waterlogged": "false"}},
        {"Name": "minecraft:glowstone", "Properties": None},
        {"Name": "minecraft:furnace", "Properties": {"facing": "north"}},
    ]
    # store order flat = y*(2*2)+z*2+x ; volume 4, 4 entries -> 2 bits, 1 word
    IDS2 = [1, 3, 0, 2]  # (0,0,0)=stairs, (1,0,0)=furnace, (0,0,1)=air, (1,0,1)=glowstone
    bits2 = bits_per_entry(len(PALETTE2))
    assert bits2 == 2, bits2
    assert packed_long_count(len(IDS2), bits2) == 1
    words2 = to_signed(pack_entries(IDS2, bits2))
    region2 = build_region(REGION2_POS, REGION2_SIZE, PALETTE2, IDS2, None)
    region2["BlockStates"] = (TAG_LONG_ARRAY, words2)
    region2["TileEntities"] = (TAG_LIST, (TAG_COMPOUND, [{
        "id": (TAG_STRING, "minecraft:furnace"),
        "x": (TAG_INT, 1), "y": (TAG_INT, 0), "z": (TAG_INT, 0),
    }]))
    non_air2 = sum(1 for i in IDS2 if i != 0)

    # EnclosingSize = union bbox over positive-size regions:
    # test: x 10..12, y 64..65, z -5..-2 ; upper: x 20..21, y 70..70, z 10..11
    ENC = (12, 7, 17)  # x 10..21, y 64..70, z -5..11

    root = {
        "Version": (TAG_INT, 7),
        "SubVersion": (TAG_INT, 1),
        "MinecraftDataVersion": (TAG_INT, DATA_VERSION),
        "Metadata": (TAG_COMPOUND, {
            "EnclosingSize": (TAG_COMPOUND, {
                "x": (TAG_INT, ENC[0]), "y": (TAG_INT, ENC[1]), "z": (TAG_INT, ENC[2])}),
            "Author": (TAG_STRING, "Architect fixture"),
            "Description": (TAG_STRING, "M2 fixture: 2 regions (3x2x4 + 2x1x2)"),
            "Name": (TAG_STRING, "small"),
            "RegionCount": (TAG_INT, 2),
            "TimeCreated": (TAG_LONG, 1720000000000),
            "TimeModified": (TAG_LONG, 1720000000000),
            "TotalBlocks": (TAG_INT, non_air + non_air2),
            "TotalVolume": (TAG_INT, VOLUME + len(IDS2)),
        }),
        "Regions": (TAG_COMPOUND, {
            "test": (TAG_COMPOUND, region),
            "upper": (TAG_COMPOUND, region2),
        }),
    }

    litematic_path = os.path.join(fixtures, "small.litematic")
    with open(litematic_path, "wb") as f:
        f.write(write_root_gzip(root))

    expected = {
        "version": 7,
        "subVersion": 1,
        "dataVersion": DATA_VERSION,
        "region": "test",
        "position": list(REGION_POS),
        "size": [SX, SY, SZ],
        "volume": VOLUME,
        "palette": [{"name": e["Name"], "properties": e["Properties"] or {}} for e in PALETTE],
        "bits": bits,
        "words": words,
        "cells": {str(i): v for i, v in enumerate(ids)},
        "tileEntities": [{"id": "minecraft:chest", "x": 0, "y": 0, "z": 3}],
        "totalBlocks": non_air,
        "region2": {
            "name": "upper",
            "position": list(REGION2_POS),
            "size": list(REGION2_SIZE),
            "volume": len(IDS2),
            "palette": [{"name": e["Name"], "properties": e["Properties"] or {}} for e in PALETTE2],
            "bits": bits2,
            "words": words2,
            "cells": {str(i): v for i, v in enumerate(IDS2)},
            "tileEntities": [{"id": "minecraft:furnace", "x": 1, "y": 0, "z": 0}],
            "totalBlocks": non_air2,
        },
        "enclosingSize": list(ENC),
        "regionCount": 2,
        "totalVolume": VOLUME + len(IDS2),
        "totalBlocksAll": non_air + non_air2,
    }
    with open(os.path.join(fixtures, "small.expected.json"), "w", encoding="utf-8") as f:
        json.dump(expected, f, indent=2)

    print("wrote %s (%d cells, %d non-air)" % (litematic_path, VOLUME + len(IDS2), non_air + non_air2))


if __name__ == "__main__":
    main()
