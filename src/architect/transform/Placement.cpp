#include "architect/transform/Placement.h"

namespace architect::transform {

Vec3i rotatedSize(Vec3i size, Rotation rotation) {
    switch (rotation) {
        case Rotation::None:
        case Rotation::Cw180:
            return size;
        case Rotation::Cw90:
        case Rotation::Cw90Ccw: {
            Vec3i out = size;
            out.x = size.z;
            out.z = size.x;
            return out;
        }
    }
    return size; // Unreachable; keeps /W4 quiet on all-paths.
}

int mirrorAxis(int value, int size, Mirror mirror) {
    if (mirror == Mirror::None) {
        return value;
    }
    return size - 1 - value;
}

Vec3i transformLocal(Vec3i cell, Vec3i size, Rotation rotation, Mirror main, Mirror sub) {
    Vec3i p = cell;
    // Litematica order: mirror(main) -> mirror(sub) -> rotate(combined).
    if (main == Mirror::X) {
        p.x = size.x - 1 - p.x;
    }
    if (main == Mirror::Z) {
        p.z = size.z - 1 - p.z;
    }
    if (sub == Mirror::X) {
        p.x = size.x - 1 - p.x;
    }
    if (sub == Mirror::Z) {
        p.z = size.z - 1 - p.z;
    }
    Vec3i q{0, p.y, 0};
    switch (rotation) {
        case Rotation::None:
            q.x = p.x;
            q.z = p.z;
            break;
        case Rotation::Cw90:
            q.x = size.z - 1 - p.z;
            q.z = p.x;
            break;
        case Rotation::Cw180:
            q.x = size.x - 1 - p.x;
            q.z = size.z - 1 - p.z;
            break;
        case Rotation::Cw90Ccw:
            q.x = p.z;
            q.z = size.x - 1 - p.x;
            break;
    }
    return q;
}

Vec3i toWorld(Vec3i cell, Vec3i regionSize, const Placement& placement) {
    const Vec3i local = transformLocal(cell, regionSize, placement.rotation, placement.mirrorMain, placement.mirrorSub);
    return Vec3i{placement.anchor.x + local.x, placement.anchor.y + local.y, placement.anchor.z + local.z};
}

} // namespace architect::transform
