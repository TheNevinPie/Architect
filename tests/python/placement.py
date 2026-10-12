"""Python mirror of src/architect/transform/Placement.h.

Same conventions: CW rotations seen from above (+Y looking down),
mirror(main) -> mirror(sub) -> rotate(combined) like Litematica's
placeBlocksToWorld. Vectors are pinned in test_transforms.py.
"""

NONE, CW90, CW180, CCW90 = 0, 1, 2, 3
M_NONE, M_X, M_Z = 0, 1, 2


def rotated_size(size, rotation):
    sx, sy, sz = size
    if rotation in (CW90, CCW90):
        return (sz, sy, sx)
    return (sx, sy, sz)


def transform_local(cell, size, rotation, main=M_NONE, sub=M_NONE):
    sx, sy, sz = size
    x, y, z = cell
    if main == M_X:
        x = sx - 1 - x
    if main == M_Z:
        z = sz - 1 - z
    if sub == M_X:
        x = sx - 1 - x
    if sub == M_Z:
        z = sz - 1 - z
    if rotation == NONE:
        q = (x, y, z)
    elif rotation == CW90:
        q = (sz - 1 - z, y, x)
    elif rotation == CW180:
        q = (sx - 1 - x, y, sz - 1 - z)
    elif rotation == CCW90:
        q = (z, y, sx - 1 - x)
    else:
        raise ValueError(rotation)
    return q


def to_world(cell, region_size, anchor, rotation=NONE, main=M_NONE, sub=M_NONE):
    lx, ly, lz = transform_local(cell, region_size, rotation, main, sub)
    ax, ay, az = anchor
    return (ax + lx, ay + ly, az + lz)
