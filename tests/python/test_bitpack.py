"""M1 COMPLETED: bit-packing vectors shared by Bits.h and litematic_bitpack.py.

Run: python tests/python/test_bitpack.py  (stdlib only, no pytest needed)
Exit non-zero on any failure.
"""

import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from litematic_bitpack import (  # noqa: E402
    bits_per_entry, flat_index_litematic, flat_index_schem_v3,
    pack_entries, packed_long_count, to_signed, unpack_entry,
)

FAILURES = []


def check(name, cond, detail=""):
    print(("PASS " if cond else "FAIL ") + name + (" | " + str(detail) if detail and not cond else ""))
    if not cond:
        FAILURES.append(name)


# bits_per_entry: max(2, ceil(log2(n)))
check("bits empty -> None", bits_per_entry(0) is None)
check("bits 1 -> 2", bits_per_entry(1) == 2)
check("bits 2 -> 2", bits_per_entry(2) == 2)
check("bits 3 -> 2", bits_per_entry(3) == 2)
check("bits 4 -> 2", bits_per_entry(4) == 2)
check("bits 5 -> 3", bits_per_entry(5) == 3)
check("bits 7(fixture) -> 3", bits_per_entry(7) == 3)
check("bits 8 -> 3", bits_per_entry(8) == 3)
check("bits 9 -> 4", bits_per_entry(9) == 4)
check("bits 16 -> 4", bits_per_entry(16) == 4)
check("bits 17 -> 5", bits_per_entry(17) == 5)

# packed_long_count
check("len 24 cells @3b = 2 words", packed_long_count(24, 3) == 2)
check("len 1 cell @2b = 1 word", packed_long_count(1, 2) == 1)
check("len 32 cells @2b = 1 word", packed_long_count(32, 2) == 1)
check("len 33 cells @2b = 2 words", packed_long_count(33, 2) == 2)

# index orders on a 3x2x4 volume
check("litematic idx (0,0,0)=0", flat_index_litematic(0, 0, 0, 3, 4) == 0)
check("litematic idx (2,0,0)=2", flat_index_litematic(2, 0, 0, 3, 4) == 2)
check("litematic idx (0,0,1)=3", flat_index_litematic(0, 0, 1, 3, 4) == 3)
check("litematic idx (0,1,0)=12", flat_index_litematic(0, 1, 0, 3, 4) == 12)
check("litematic idx (0,1,3)=21 straddle", flat_index_litematic(0, 1, 3, 3, 4) == 21)
check("litematic idx (2,1,3)=23 last", flat_index_litematic(2, 1, 3, 3, 4) == 23)
check("schemv3 idx (1,0,0)=1", flat_index_schem_v3(1, 0, 0, 3, 4) == 1)
check("schemv3 idx (0,1,0)=12", flat_index_schem_v3(0, 1, 0, 3, 4) == 12)
check("schemv3 idx (0,0,1)=3", flat_index_schem_v3(0, 0, 1, 3, 4) == 3)

# round-trip incl. straddling entries, randomized against many shapes
rng = random.Random(1234)
ok = True
for trial in range(300):
    pal = rng.randint(1, 40)
    bits = bits_per_entry(pal)
    vol = rng.randint(1, 200)
    ids = [rng.randrange(pal) for _ in range(vol)]
    signed = to_signed(pack_entries(ids, bits))
    if len(signed) != packed_long_count(vol, bits):
        ok = False
        break
    for i, want in enumerate(ids):
        if unpack_entry(signed, i, bits, pal) != want:
            ok = False
            break
    if not ok:
        break
check("300 randomized pack/unpack round-trips", ok)

# straddle pin: 3-bit entries, index 21 spans words 0/1
ids = [0] * 24
ids[21] = 5
signed = to_signed(pack_entries(ids, 3))
check("straddle word count = 2", len(signed) == 2)
check("straddle idx21 = 5", unpack_entry(signed, 21, 3, 7) == 5)
check("straddle neighbor idx20 = 0", unpack_entry(signed, 20, 3, 7) == 0)
check("straddle neighbor idx22 = 0", unpack_entry(signed, 22, 3, 7) == 0)

# rejection paths (mirror readLitematic hard errors)
check("short array -> None", unpack_entry(to_signed([0]), 40, 3, 7) is None)
check("id >= palette -> None", unpack_entry(to_signed(pack_entries([7], 3)), 0, 3, 7) is None)
check("bits 0 -> None", unpack_entry(to_signed([0]), 0, 0, 7) is None)

print("%d failure(s)" % len(FAILURES))
sys.exit(1 if FAILURES else 0)
