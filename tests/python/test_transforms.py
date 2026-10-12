"""M1 COMPLETED: placement vectors shared by Placement.h/cpp and placement.py.

Run: python tests/python/test_transforms.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from placement import (  # noqa: E402
    CCW90, CW90, CW180, M_NONE, M_X, M_Z, NONE,
    rotated_size, to_world, transform_local,
)

FAILURES = []


def check(name, cond, detail=""):
    print(("PASS " if cond else "FAIL ") + name + (" | " + str(detail) if detail and not cond else ""))
    if not cond:
        FAILURES.append(name)


S = (3, 2, 4)  # fixture region size (non-cubic)

# size swap on odd quarter turns
check("rotatedSize none", rotated_size(S, NONE) == (3, 2, 4))
check("rotatedSize cw90 swaps", rotated_size(S, CW90) == (4, 2, 3))
check("rotatedSize cw180 keeps", rotated_size(S, CW180) == (3, 2, 4))
check("rotatedSize ccw90 swaps", rotated_size(S, CCW90) == (4, 2, 3))

# identity
check("identity (0,0,0)", transform_local((0, 0, 0), S, NONE) == (0, 0, 0))
check("identity (2,1,3)", transform_local((2, 1, 3), S, NONE) == (2, 1, 3))

# hand-derived CW90 corners: (x,z) -> (sz-1-z, x), size (3,2,4)
check("cw90 (0,0,0)->(3,0,0)", transform_local((0, 0, 0), S, CW90) == (3, 0, 0))
check("cw90 (2,1,3)->(0,1,2)", transform_local((2, 1, 3), S, CW90) == (0, 1, 2))
check("cw90 (0,0,3)->(0,0,0)", transform_local((0, 0, 3), S, CW90) == (0, 0, 0))

# CW180: (sx-1-x, sz-1-z)
check("cw180 (0,0,0)->(2,0,3)", transform_local((0, 0, 0), S, CW180) == (2, 0, 3))
check("cw180 (2,1,3)->(0,1,0)", transform_local((2, 1, 3), S, CW180) == (0, 1, 0))

# CCW90: (z, sx-1-x)
check("ccw90 (0,0,0)->(0,0,2)", transform_local((0, 0, 0), S, CCW90) == (0, 0, 2))
check("ccw90 (2,1,3)->(3,1,0)", transform_local((2, 1, 3), S, CCW90) == (3, 1, 0))

# mirrors
check("mirror X (0,0,0)->(2,0,0)", transform_local((0, 0, 0), S, NONE, M_X) == (2, 0, 0))
check("mirror Z (0,0,0)->(0,0,3)", transform_local((0, 0, 0), S, NONE, M_Z) == (0, 0, 3))
check("mirror X twice = identity",
      transform_local(transform_local((1, 0, 2), S, NONE, M_X), S, NONE, M_X) == (1, 0, 2))

# Litematica order: mirror(main) -> mirror(sub) -> rotate(combined).
# main=X then cw90 on (0,0,0): mirror -> (2,0,0); rotate -> (3,0,2).
check("order mirror-then-rotate",
      transform_local((0, 0, 0), S, CW90, M_X) == (3, 0, 2))
# rotate-then-mirror would give (1,0,0); assert we do NOT produce that.
check("order is not rotate-then-mirror",
      transform_local((0, 0, 0), S, CW90, M_X) != (1, 0, 0))

# rotation composition: 4x CW90 = identity over the whole region
cells = [(x, y, z) for x in range(3) for y in range(2) for z in range(4)]
back = True
for c in cells:
    p = transform_local(c, S, CW90)
    p = transform_local(p, (4, 2, 3), CW90)
    p = transform_local(p, (3, 2, 4), CW90)
    p = transform_local(p, (4, 2, 3), CW90)
    if p != c:
        back = False
        break
check("4x cw90 = identity (all 24 cells)", back)

# CW90 then CCW90 = identity
back2 = all(
    transform_local(transform_local(c, S, CW90), (4, 2, 3), CCW90) == c for c in cells
)
check("cw90+ccw90 = identity (all 24 cells)", back2)

# every transform stays inside the rotated bounds (all cells, all rotations)
inside = True
for rot, rs in ((NONE, S), (CW90, (4, 2, 3)), (CW180, S), (CCW90, (4, 2, 3))):
    for c in cells:
        q = transform_local(c, S, rot)
        if not (0 <= q[0] < rs[0] and 0 <= q[1] < rs[1] and 0 <= q[2] < rs[2]):
            inside = False
check("all rotations stay in rotated bounds", inside)

# toWorld anchors the transformed origin
check("toWorld anchor+identity",
      to_world((1, 0, 1), S, (10, 64, -5)) == (11, 64, -4))
check("toWorld anchor+cw90 corner",
      to_world((0, 0, 0), S, (10, 64, -5), CW90) == (13, 64, -5))

print("%d failure(s)" % len(FAILURES))
sys.exit(1 if FAILURES else 0)
