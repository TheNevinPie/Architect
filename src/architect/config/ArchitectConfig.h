#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace architect::config {

// Plain-data mod configuration. Serialization (ll/api/Config.h JSON +
// Mod::getDataDir/getConfigDir persistence, per-world placements keyed by
// world/server name) is the game adapter's job; this struct is the shared
// schema so adapters and UI cannot drift apart.
struct ArchitectConfig {
    int version = 1;

    // Verifier budget: max BlockSource reads per game tick.
    std::uint64_t verifyBudgetPerTick = 4000;

    // Render caps: warn above, refuse to snapshot above maxGhostVolume.
    std::uint64_t warnGhostVolume = 128ULL * 128ULL * 128ULL;
    std::uint64_t maxGhostVolume = 256ULL * 256ULL * 256ULL;

    // Blueprint library + last placements.
    std::string schematicDir = "schematics";
    struct SavedPlacement {
        std::string fileName;
        std::string worldKey; // world/server name from ClientStartJoinLevelEvent
        int anchorX = 0;
        int anchorY = 0;
        int anchorZ = 0;
        int rotation = 0; // quarter turns CW
    };
    std::vector<SavedPlacement> placements;
};

} // namespace architect::config
