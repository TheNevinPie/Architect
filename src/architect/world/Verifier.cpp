#include "architect/world/Verifier.h"

#include <algorithm>

namespace architect::world {

DiffResult diffIncremental(
    const std::unordered_map<Vec3i, BlockState, Vec3iHash>& expected,
    const BlockQuery&                                       query,
    std::uint64_t                                           budget
) {
    DiffResult out;
    if (!query || budget == 0 || expected.empty()) {
        return out;
    }

    // Deterministic order so budget slicing is stable across ticks.
    std::vector<Vec3i> keys;
    keys.reserve(expected.size());
    for (const auto& [pos, _] : expected) {
        keys.push_back(pos);
    }
    std::sort(keys.begin(), keys.end(), [](const Vec3i& a, const Vec3i& b) {
        if (a.y != b.y) {
            return a.y < b.y;
        }
        if (a.z != b.z) {
            return a.z < b.z;
        }
        return a.x < b.x;
    });

    const std::uint64_t count = std::min<std::uint64_t>(budget, keys.size());
    for (std::uint64_t i = 0; i < count; ++i) {
        const Vec3i& pos = keys[i];
        const BlockState& want = expected.at(pos);
        const ObservedCell got = query(pos);

        CellStatus status;
        if (!got.chunkLoaded) {
            status = CellStatus::Unchecked;
        } else if (want.isAir) {
            // Expected air/void: Correct only when the world is also air.
            // (Out-of-footprint extras are found by the adapter's bounds
            // scan in M2; this core pass covers the schematic footprint.)
            status = got.isAir ? CellStatus::Correct : CellStatus::Extra;
        } else if (got.isAir) {
            status = CellStatus::Missing;
        } else if (got.bedrockName != want.name) {
            // NOTE: names here are compared post-mapping (both Bedrock
            // namespace). The adapter maps before building `expected`.
            status = CellStatus::Incorrect;
        } else {
            bool statesMatch = true;
            for (const auto& [key, value] : want.states) {
                const auto it = got.bedrockStates.find(key);
                if (it == got.bedrockStates.end() || it->second != value) {
                    statesMatch = false;
                    break;
                }
            }
            status = statesMatch ? CellStatus::Correct : CellStatus::Incorrect;
        }

        switch (status) {
            case CellStatus::Missing: out.stats.missing++; break;
            case CellStatus::Correct: out.stats.correct++; break;
            case CellStatus::Incorrect: out.stats.incorrect++; break;
            case CellStatus::Extra: out.stats.extra++; break;
            case CellStatus::Unchecked: out.stats.unchecked++; break;
        }
        if (status != CellStatus::Correct) {
            out.cells.emplace(pos, status);
        }
    }
    return out;
}

} // namespace architect::world
