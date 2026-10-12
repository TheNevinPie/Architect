#pragma once

#include "architect/core/Schematic.h"

#include <string>

namespace architect::mapping {

// Outcome of translating one source-namespace block state to Bedrock.
// The verifier and renderer must handle all four; silent air substitution
// is forbidden (a miss is a visible placeholder + counted warning).
enum class MapResultKind {
    Exact,       // name + all states matched 1:1
    Partial,     // name matched; only allowlisted states carried, rest defaulted
    Placeholder, // no usable match; render/compare as marked placeholder
    Unsupported, // structurally unmappable (caller decides: placeholder + error)
};

struct MappedBlock {
    MapResultKind kind = MapResultKind::Unsupported;
    std::string bedrockName;                          // e.g. "minecraft:stone"
    std::map<std::string, std::string> bedrockStates; // Bedrock-typed property values
    std::string note; // why partial/placeholder (shown in material report)
};

// Translates one decoded source state to Bedrock. Backed by the curated
// JSON table (tools/prune_blockmap.py derives it from PrismarineJS
// minecraft-data blocksJ2B.json pinned to one game version). M2 scope:
// declared here to fix the boundary; the table ships with the verifier.
[[nodiscard]] MappedBlock mapJavaToBedrock(const BlockState& java);

} // namespace architect::mapping
