#pragma once

#include "architect/core/Vec3i.h"

#include <cstdint>

namespace architect::transform {

// Own rotation/mirror vocabulary for schematic placement. These describe
// what the USER asked for; translation into game helpers
// (BlockPos::transform, VanillaBlockStateTransformUtils) happens in the
// game-facing adapter, not here, so this math stays testable without the SDK.
enum class Rotation : std::uint8_t {
    None = 0, // 0 deg
    Cw90 = 1, // 90 deg clockwise seen from above (+Y looking down, -Y view dir)
    Cw180 = 2,
    Cw90Ccw = 3, // 270 CW == 90 CCW
};

enum class Mirror : std::uint8_t {
    None = 0,
    X = 1, // flip the local X axis
    Z = 2, // flip the local Z axis
};

struct Placement {
    Vec3i anchor{0, 0, 0}; // world position of the schematic origin
    Rotation rotation = Rotation::None;
    Mirror mirrorMain = Mirror::None; // applied first  (Litematica "main")
    Mirror mirrorSub = Mirror::None;  // applied second (Litematica "sub")
    // Mirror-then-rotate order matches Litematica (placeBlocksToWorld):
    // mirror(main) -> mirror(sub) -> rotate(combined), with axis swap at
    // +/-90 deg handled by rotating the already-mirrored offsets.
};

// Size of a region after `rotation` (W/L swap on odd quarter turns).
[[nodiscard]] Vec3i rotatedSize(Vec3i size, Rotation rotation);

// Mirror one axis of an offset inside [0, size).
[[nodiscard]] int mirrorAxis(int value, int size, Mirror mirror);

// Full local transform: mirror main, mirror sub, then rotate about the
// region origin. `size` is the PRE-rotation region size.
[[nodiscard]] Vec3i transformLocal(Vec3i cell, Vec3i size, Rotation rotation, Mirror main, Mirror sub);

// World position of a region-local cell under a placement.
[[nodiscard]] Vec3i toWorld(Vec3i cell, Vec3i regionSize, const Placement& placement);

// Directional-state rotation (facings, hinges, rotation:0-15, ...) is NOT
// computed here: the adapter applies the game's own
// VanillaBlockStateTransformUtils to the mapped Bedrock states. This header
// only moves positions. See docs/MODULE_BOUNDARIES.md.

} // namespace architect::transform
