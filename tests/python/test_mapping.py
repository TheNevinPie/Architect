"""M2 COMPLETED: mapping policy mirror (hand-computed expectations).

Mirrors the policy in src/architect/mapping/BlockMap.cpp:
Exact = name kept + no dropped state; Partial = allowlisted carry with a
note; Placeholder = non-vanilla/empty (never silent air).

Run: python tests/python/test_mapping.py
"""

import os
import sys

FAILURES = []


def check(name, cond, detail=""):
    print(("PASS " if cond else "FAIL ") + name + (" | " + str(detail) if detail and not cond else ""))
    if not cond:
        FAILURES.append(name)


CARRIED = {
    "minecraft:oak_stairs": {"facing", "half", "shape"},
    "minecraft:chest": {"facing", "type"},
    "minecraft:oak_log": {"axis"},
    "minecraft:wall_torch": {"facing"},
}


def map_java(name, states):
    """Independent port of the BlockMap.cpp policy (contract pin)."""
    if not name:
        return ("placeholder", name, "")
    if name in ("minecraft:air", "minecraft:cave_air", "minecraft:void_air"):
        return ("exact", "minecraft:air", "")
    if not name.startswith("minecraft:"):
        return ("placeholder", name, "non-vanilla namespace")
    kept, dropped, water = {}, None, False
    for k, v in states.items():
        if k == "waterlogged":
            if v == "true":
                water = True
            dropped = dropped or "waterlogged"
            continue
        if k in CARRIED.get(name, set()):
            kept[k] = v
        elif dropped is None:
            dropped = k
    if not dropped and not water:
        return ("exact", name, "")
    note = "waterlogged=true needs water on the Bedrock second layer" if water else \
        "unmapped properties dropped: %s" % dropped
    return ("partial", name, note)


cases = [
    (("minecraft:air", {}), ("exact", "minecraft:air", "")),
    (("minecraft:stone", {}), ("exact", "minecraft:stone", "")),
    (("minecraft:oak_stairs", {"facing": "east", "half": "bottom", "shape": "straight"}),
     ("exact", "minecraft:oak_stairs", "")),
    (("minecraft:oak_stairs", {"facing": "east", "waterlogged": "true"}), None),  # partial+layer note
    (("minecraft:oak_stairs", {"facing": "east", "crackLevel": "3"}), None),      # partial+names key
    (("biomeswevegone:ebony_planks", {}), ("placeholder", "biomeswevegone:ebony_planks", "")),
    (("", {}), ("placeholder", "", "")),
    (("minecraft:oak_log", {"axis": "y"}), ("exact", "minecraft:oak_log", "")),
]
for (name, states), want in cases:
    kind, out_name, _ = got = map_java(name, states)
    if want is None:
        check("partial %s" % name, kind == "partial" and out_name == name, got)
        if name == "minecraft:oak_stairs" and states.get("waterlogged") == "true":
            check("water layer note", "second layer" in got[2], got)
        if "crackLevel" in states:
            check("dropped key named", "crackLevel" in got[2], got)
    else:
        check("%s -> %s" % (name, want[0]), (kind, out_name) == (want[0], want[1]), got)

# every fixture palette entry classifies without air-substitution
for f in ["minecraft:oak_stairs", "minecraft:glass", "minecraft:oak_log",
          "minecraft:wall_torch", "minecraft:chest"]:
    kind, out_name, _ = map_java(f, {"facing": "east"})
    check("fixture %s never air" % f, out_name == f and kind in ("exact", "partial"))

print("%d failure(s)" % len(FAILURES))
sys.exit(1 if FAILURES else 0)
