#pragma once

#include "architect/core/Vec3i.h"
#include "architect/world/Verifier.h"

#include <array>
#include <cstdint>
#include <vector>

namespace architect::render {

// Thread-handoff snapshot. The verifier (game thread) publishes these;
// the overlay (render thread / Present hook) draws ONLY from the latest
// published snapshot. No game pointers cross the boundary: plain positions
// + status colours. See docs/RENDER_POC.md for the threading contract.
struct Box {
    Vec3i min{0, 0, 0};
    Vec3i max{0, 0, 0}; // inclusive
    world::CellStatus status = world::CellStatus::Unchecked;
};

// Status -> overlay colour (single canonical palette; both the DX overlay
// and the future in-pipeline wireframes use it).
struct Color {
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
};

[[nodiscard]] Color colorFor(world::CellStatus status);

// Merged per-chunk line/box lists published as one atomic snapshot.
// Chunked from day one so far-field buckets can be simplified later
// (hybrid cheap-far/detailed-near) without changing the handoff.
struct ChunkBoxes {
    int chunkX = 0;
    int chunkZ = 0;
    std::vector<Box> boxes;
};

struct RenderSnapshot {
    std::uint64_t sequence = 0; // monotonically increasing; overlay skips stale
    std::vector<ChunkBoxes> chunks;
};

} // namespace architect::render
