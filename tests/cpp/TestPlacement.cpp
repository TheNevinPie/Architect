// M1 PROPOSED (runs under xmake once the toolchain lands).
// Mirrors tests/python/test_transforms.py vectors against
// architect/transform/Placement.h. stdlib only.
#include "architect/transform/Placement.h"

#include <cstdio>

namespace {

int failures = 0;

void check(const char* name, bool cond) {
    std::printf("%s %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) {
        ++failures;
    }
}

} // namespace

int testPlacement() {
    using namespace architect;
    using namespace architect::transform;

    const Vec3i s{3, 2, 4};
    check("rotatedSize cw90 swaps", rotatedSize(s, Rotation::Cw90) == Vec3i{4, 2, 3});
    check("rotatedSize cw180 keeps", rotatedSize(s, Rotation::Cw180) == s);
    check("identity", transformLocal({0, 0, 0}, s, Rotation::None, Mirror::None, Mirror::None) == Vec3i{0, 0, 0});
    check(
        "cw90 (0,0,0)->(3,0,0)",
        transformLocal({0, 0, 0}, s, Rotation::Cw90, Mirror::None, Mirror::None) == Vec3i{3, 0, 0}
    );
    check(
        "cw90 (2,1,3)->(0,1,2)",
        transformLocal({2, 1, 3}, s, Rotation::Cw90, Mirror::None, Mirror::None) == Vec3i{0, 1, 2}
    );
    check(
        "cw180 (0,0,0)->(2,0,3)",
        transformLocal({0, 0, 0}, s, Rotation::Cw180, Mirror::None, Mirror::None) == Vec3i{2, 0, 3}
    );
    check(
        "ccw90 (2,1,3)->(3,1,0)",
        transformLocal({2, 1, 3}, s, Rotation::Cw90Ccw, Mirror::None, Mirror::None) == Vec3i{3, 1, 0}
    );
    check(
        "mirror X (0,0,0)->(2,0,0)",
        transformLocal({0, 0, 0}, s, Rotation::None, Mirror::X, Mirror::None) == Vec3i{2, 0, 0}
    );
    check(
        "order mirror-then-rotate",
        transformLocal({0, 0, 0}, s, Rotation::Cw90, Mirror::X, Mirror::None) == Vec3i{3, 0, 2}
    );

    // 4x Cw90 over the whole region returns every cell home.
    bool back = true;
    for (int x = 0; x < 3 && back; ++x) {
        for (int y = 0; y < 2 && back; ++y) {
            for (int z = 0; z < 4 && back; ++z) {
                Vec3i p{x, y, z};
                p = transformLocal(p, {3, 2, 4}, Rotation::Cw90, Mirror::None, Mirror::None);
                p = transformLocal(p, {4, 2, 3}, Rotation::Cw90, Mirror::None, Mirror::None);
                p = transformLocal(p, {3, 2, 4}, Rotation::Cw90, Mirror::None, Mirror::None);
                p = transformLocal(p, {4, 2, 3}, Rotation::Cw90, Mirror::None, Mirror::None);
                back = (p == Vec3i{x, y, z});
            }
        }
    }
    check("4x cw90 = identity (all 24 cells)", back);

    Placement place;
    place.anchor = Vec3i{10, 64, -5};
    place.rotation = Rotation::Cw90;
    check("toWorld anchor+cw90", toWorld({0, 0, 0}, s, place) == Vec3i{13, 64, -5});
    return failures;
}
