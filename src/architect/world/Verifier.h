#pragma once

#include "architect/core/Schematic.h"
#include "architect/core/Vec3i.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace architect::world {

// Per-cell comparison outcome. Every read is chunk-gated by the adapter:
// positions in unloaded chunks report Unchecked, never Correct.
enum class CellStatus : std::uint8_t {
    Missing,   // schematic expects a block, world has air
    Correct,   // name + compared states match
    Incorrect, // present but name or a compared state differs
    Extra,     // world has a block where the schematic expects air/void
    Unchecked, // chunk not loaded (or Y out of range): unknown, not correct
};

// What the game-facing adapter observes at one world position.
// Produced on the GAME THREAD only (BlockSource is not thread-safe);
// the verifier never touches the game from render/worker threads.
struct ObservedCell {
    bool chunkLoaded = false;
    bool isAir = true;
    std::string bedrockName;                          // valid when !isAir
    std::map<std::string, std::string> bedrockStates; // sparse compared states
};

// Game-thread callback implemented by the adapter (BlockSource reads).
// Pure std::function so the verifier core is unit-testable with a fake.
using BlockQuery = std::function<ObservedCell(const Vec3i& worldPos)>;

struct DiffStats {
    std::uint64_t missing = 0;
    std::uint64_t correct = 0;
    std::uint64_t incorrect = 0;
    std::uint64_t extra = 0;
    std::uint64_t unchecked = 0;
};

struct DiffResult {
    // Sparse: only non-Correct cells are stored (correct cells are the
    // common case and would bloat memory on 100^3 builds).
    std::unordered_map<Vec3i, CellStatus, Vec3iHash> cells;
    DiffStats stats;
};

// Compares expected world-space cells against the game via `query`.
// `expected` maps world pos -> source state (post placement transform,
// post mapping). `budget` caps BlockSource reads per call so a 10^6-cell
// schematic is verified incrementally across ticks, not in one frame.
// Pure function over the callback: deterministic and testable.
[[nodiscard]] DiffResult diffIncremental(
    const std::unordered_map<Vec3i, BlockState, Vec3iHash>& expected,
    const BlockQuery&                                       query,
    std::uint64_t                                           budget
);

} // namespace architect::world
