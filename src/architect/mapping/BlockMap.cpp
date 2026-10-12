// M2: curated Java->Bedrock map. Policy (deterministic, no silent air):
//   - "minecraft:air" (or isAir) -> Exact air.
//   - Same-namespace passthrough: "minecraft:X" stays "minecraft:X"
//     (no renames curated yet — inventing any would violate the
//     no-speculation rule; verified renames land here with evidence).
//     States: only per-block allowlisted properties are carried; any
//     dropped property demotes Exact -> Partial with a note naming it.
//     waterlogged=true is NEVER silently dropped: Partial + note that
//     Bedrock needs water on the second layer (verifier compares both).
//   - Non-minecraft namespaces or empty names -> Placeholder (keeps the
//     source name, never air).
// The full PrismarineJS-derived table is deferred; this curated policy
// covers the M2 fixture states exactly and degrades visibly otherwise.
#include "architect/mapping/BlockMap.h"

#include <unordered_map>
#include <unordered_set>

namespace architect::mapping {
namespace {

// No rename table in M2: no Java->Bedrock rename has been verified
// against a source yet, and inventing one would violate the
// no-speculation rule. Same-namespace passthrough applies; verified
// renames land here with their evidence cited.

// Properties carried per block (Java value domains pass through; the
// game adapter normalizes domains like facing strings at compare time).
const std::unordered_map<std::string, std::unordered_set<std::string>>& carriedStates() {
    static const std::unordered_map<std::string, std::unordered_set<std::string>> table = {
        {"minecraft:oak_stairs", {"facing", "half", "shape"}},
        {"minecraft:chest", {"facing", "type"}},
        {"minecraft:oak_log", {"axis"}},
        {"minecraft:wall_torch", {"facing"}},
    };
    return table;
}

} // namespace

MappedBlock mapJavaToBedrock(const BlockState& java) {
    MappedBlock out;
    if (java.name.empty()) {
        out.kind = MapResultKind::Placeholder;
        out.note = "empty block name";
        return out;
    }
    if (java.isAir || java.name == "minecraft:air" || java.name == "minecraft:cave_air" ||
        java.name == "minecraft:void_air") {
        out.kind        = MapResultKind::Exact;
        out.bedrockName = "minecraft:air";
        return out;
    }
    constexpr const char* kPrefix = "minecraft:";
    constexpr std::size_t kPrefixLen = 10;
    if (java.name.compare(0, kPrefixLen, kPrefix) != 0) {
        out.kind        = MapResultKind::Placeholder;
        out.bedrockName = java.name;
        out.note        = "non-vanilla namespace; no curated mapping";
        return out;
    }
    out.bedrockName = java.name; // same-namespace passthrough (no renames curated in M2).

    const auto allowedIt = carriedStates().find(java.name);
    bool dropped = false;
    std::string droppedName;
    bool needsWaterLayer = false;
    for (const auto& [key, value] : java.states) {
        if (key == "waterlogged") {
            if (value == "true") {
                needsWaterLayer = true;
            }
            // waterlogged is structural (Bedrock second layer), never a
            // carried property: carrying it would compare wrong.
            dropped     = true;
            droppedName = key;
            continue;
        }
        const bool carried = (allowedIt != carriedStates().end()) && allowedIt->second.count(key) != 0;
        if (carried) {
            out.bedrockStates[key] = value;
        } else if (!dropped) {
            dropped     = true;
            droppedName = key;
        }
    }
    if (!dropped && !needsWaterLayer) {
        out.kind = MapResultKind::Exact;
        return out;
    }
    out.kind = MapResultKind::Partial;
    out.note = needsWaterLayer ? "waterlogged=true needs water on the Bedrock second layer"
                              : "unmapped properties dropped: " + droppedName;
    return out;
}

} // namespace architect::mapping
