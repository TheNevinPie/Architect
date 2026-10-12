"""M1 COMPLETED: verifier taxonomy contract (mirrors world/Verifier.cpp).

Hand-computed scenarios for the 5-state taxonomy + tick-budget
determinism. The C++ diffIncremental MUST satisfy these same cases.

Run: python tests/python/test_verifier.py
"""

import os
import sys

FAILURES = []


def check(name, cond, detail=""):
    print(("PASS " if cond else "FAIL ") + name + (" | " + str(detail) if detail and not cond else ""))
    if not cond:
        FAILURES.append(name)


# Independent straightforward port of the Verifier.h contract
# (Missing/Correct/Incorrect/Extra/Unchecked; deterministic y,z,x order;
# budget caps reads; only non-Correct cells stored).
MISSING, CORRECT, INCORRECT, EXTRA, UNCHECKED = "missing", "correct", "incorrect", "extra", "unchecked"


def diff(expected, query, budget):
    stats = {MISSING: 0, CORRECT: 0, INCORRECT: 0, EXTRA: 0, UNCHECKED: 0}
    cells = {}
    keys = sorted(expected, key=lambda p: (p[1], p[2], p[0]))
    for pos in keys[:budget]:
        want_name, want_states, want_air = expected[pos]
        got = query(pos)
        if not got["loaded"]:
            s = UNCHECKED
        elif want_air:
            s = CORRECT if got["air"] else EXTRA
        elif got["air"]:
            s = MISSING
        elif got["name"] != want_name:
            s = INCORRECT
        else:
            s = CORRECT
            for k, v in want_states.items():
                if got["states"].get(k) != v:
                    s = INCORRECT
                    break
        stats[s] += 1
        if s != CORRECT:
            cells[pos] = s
    return stats, cells


STONE = ("minecraft:stone", {}, False)
AIR = ("minecraft:air", {}, True)
STAIRS = ("minecraft:oak_stairs", {"facing": "east", "half": "bottom"}, False)


def obs(loaded=True, air=True, name="", states=None):
    return {"loaded": loaded, "air": air, "name": name, "states": states or {}}


# 1. all-correct schematic stores nothing
stats, cells = diff({(0, 0, 0): STONE},
                    lambda p: obs(air=False, name="minecraft:stone"), 100)
check("correct counted", stats[CORRECT] == 1, stats)
check("correct cells not stored", cells == {})

# 2. missing
stats, cells = diff({(0, 0, 0): STONE}, lambda p: obs(air=True), 100)
check("missing", stats[MISSING] == 1 and cells[(0, 0, 0)] == MISSING)

# 3. wrong name -> incorrect
stats, cells = diff({(0, 0, 0): STONE},
                    lambda p: obs(air=False, name="minecraft:dirt"), 100)
check("wrong name incorrect", cells.get((0, 0, 0)) == INCORRECT)

# 4. right name, wrong state -> incorrect; extra observed states ignored
stats, cells = diff({(0, 0, 0): STAIRS},
                    lambda p: obs(air=False, name="minecraft:oak_stairs",
                                  states={"facing": "west", "half": "bottom"}), 100)
check("wrong state incorrect", cells.get((0, 0, 0)) == INCORRECT)
stats, _ = diff({(0, 0, 0): STONE},
                lambda p: obs(air=False, name="minecraft:stone",
                              states={"uncompared": "x"}), 100)
check("extra observed states ignored", stats[CORRECT] == 1, stats)

# 5. expected air vs world block -> extra; air vs air -> correct
stats, cells = diff({(0, 0, 0): AIR},
                    lambda p: obs(air=False, name="minecraft:stone"), 100)
check("block-in-air-slot extra", cells.get((0, 0, 0)) == EXTRA)
stats, _ = diff({(0, 0, 0): AIR}, lambda p: obs(air=True), 100)
check("air-in-air-slot correct", stats[CORRECT] == 1)

# 6. unloaded chunk -> unchecked (never correct)
stats, cells = diff({(0, 0, 0): STONE}, lambda p: obs(loaded=False), 100)
check("unloaded unchecked", cells.get((0, 0, 0)) == UNCHECKED)
check("unloaded not correct", stats[CORRECT] == 0)

# 7. budget: deterministic slicing in y,z,x order across calls
big = {(x, 0, 0): STONE for x in range(10)}
seen = []
def counting(p):
    seen.append(p)
    return obs(name="minecraft:stone")
diff(big, counting, 4)
check("budget caps reads", len(seen) == 4, seen)
check("budget order deterministic x", seen == [(0, 0, 0), (1, 0, 0), (2, 0, 0), (3, 0, 0)], seen)

# 8. mixed rig: one of each status
rig = {
    (0, 0, 0): STONE,   # correct
    (1, 0, 0): STONE,   # missing
    (2, 0, 0): STONE,   # incorrect (dirt)
    (3, 0, 0): AIR,     # extra (stone present)
    (4, 0, 0): STONE,   # unchecked
}
def rig_query(p):
    x = p[0]
    if x == 0: return obs(air=False, name="minecraft:stone")
    if x == 1: return obs(air=True)
    if x == 2: return obs(air=False, name="minecraft:dirt")
    if x == 3: return obs(air=False, name="minecraft:stone")
    return obs(loaded=False)
stats, cells = diff(rig, rig_query, 100)
check("rig stats", (stats[CORRECT], stats[MISSING], stats[INCORRECT], stats[EXTRA], stats[UNCHECKED]) == (1, 1, 1, 1, 1), stats)
check("rig sparse cells", len(cells) == 4 and (0, 0, 0) not in cells)

print("%d failure(s)" % len(FAILURES))
sys.exit(1 if FAILURES else 0)
