// M1 PROPOSED (runs under xmake once the toolchain lands).
// Mirrors the hand-computed rig in tests/python/test_verifier.py against
// architect/world/Verifier.cpp with a fake BlockQuery. stdlib only.
#include "architect/world/Verifier.h"

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

int testVerifier() {
    using namespace architect;
    using namespace architect::world;

    const BlockState stone{"minecraft:stone", {}, false};
    const BlockState air{"minecraft:air", {}, true};

    // Rig: correct / missing / incorrect / extra / unchecked.
    const std::unordered_map<Vec3i, BlockState, Vec3iHash> expected = {
        {{0, 0, 0}, stone},
        {{1, 0, 0}, stone},
        {{2, 0, 0}, stone},
        {{3, 0, 0}, air},
        {{4, 0, 0}, stone},
    };
    const BlockQuery query = [](const Vec3i& pos) -> ObservedCell {
        ObservedCell cell;
        switch (pos.x) {
            case 0: // correct
                cell.chunkLoaded = true;
                cell.isAir       = false;
                cell.bedrockName = "minecraft:stone";
                break;
            case 1: // missing
                cell.chunkLoaded = true;
                cell.isAir       = true;
                break;
            case 2: // incorrect
                cell.chunkLoaded = true;
                cell.isAir       = false;
                cell.bedrockName = "minecraft:dirt";
                break;
            case 3: // extra
                cell.chunkLoaded = true;
                cell.isAir       = false;
                cell.bedrockName = "minecraft:stone";
                break;
            default: // unchecked
                cell.chunkLoaded = false;
                break;
        }
        return cell;
    };

    const DiffResult result = diffIncremental(expected, query, 100);
    check(
        "rig stats 1/1/1/1/1",
        result.stats.correct == 1 && result.stats.missing == 1 && result.stats.incorrect == 1 &&
            result.stats.extra == 1 && result.stats.unchecked == 1
    );
    check("sparse cells == 4", result.cells.size() == 4);
    check("correct cell not stored", result.cells.find(Vec3i{0, 0, 0}) == result.cells.end());

    // Budget slicing is deterministic.
    std::unordered_map<Vec3i, BlockState, Vec3iHash> line;
    for (int x = 0; x < 10; ++x) {
        line[{x, 0, 0}] = stone;
    }
    int reads = 0;
    const BlockQuery counting = [&](const Vec3i&) -> ObservedCell {
        ++reads;
        ObservedCell cell;
        cell.chunkLoaded = true;
        cell.isAir       = false;
        cell.bedrockName = "minecraft:stone";
        return cell;
    };
    const DiffResult budgeted = diffIncremental(line, counting, 4);
    check("budget caps reads at 4", reads == 4);
    check("budgeted pass all correct", budgeted.stats.correct == 4);
    return failures;
}
