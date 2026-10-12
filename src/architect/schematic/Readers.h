#pragma once

#include "architect/core/Schematic.h"
#include "architect/nbt/Nbt.h"

#include <string>
#include <variant>
#include <vector>

namespace architect::schematic {

struct ReadError {
    std::string message;   // human-readable, shown in chat/HUD
    std::string file;      // offending file name
};

using ReadResult = std::variant<Schematic, ReadError>;

// Reads a Java Litematica schematic (.litematic: gzip + big-endian NBT).
//
// Expected layout (grounded in LitematicaSchematic.java + litemapy):
//   root: MinecraftDataVersion(int), Version(int, accept 2..7, current 7),
//         Metadata{Author, Description, Name, TimeCreated, TimeModified,
//                  RegionCount, TotalVolume, TotalBlocks},
//         Regions{ <name>: {Position{x,y,z}, Size{x,y,z} (signed),
//                  BlockStates(long[]), Palette[list of {Name, Properties?}],
//                  TileEntities[list], PendingBlockTicks[list, v>=3],
//                  PendingFluidTicks[list, v>=5]} }
// Cell index: y * (sizeX * sizeZ) + z * sizeX + x (x fastest), sizes
// normalized with abs(). Palette id 0 is conventionally air.
//
// Rejections (hard errors, never silent): Version == 1 or > 7; empty
// palette; BlockStates length != ceil(volume*bits/64); palette id out of
// range. Unknown future sub-versions warn but parse.
[[nodiscard]] ReadResult readLitematic(const std::vector<std::uint8_t>& fileBytes);

// Same decoder over an already-parsed root (no gzip needed): the unit
// under test (tests/cpp/TestReader.cpp) and the target for the Python
// conformance suite. M2 IMPLEMENTED.
[[nodiscard]] ReadResult readLitematicFromRoot(const nbt::Compound& root);

// Reads a Sponge schematic v2/v3 (.schem: gzip + big-endian NBT).
// v3 nests under "Schematic" with Blocks{Palette, Data(varint), BlockEntities}.
// Cell index: x + z * sizeX + y * sizeX * sizeZ.
// v2 (root-is-schematic) falls out of the same decoder. M2 scope: declared
// here so the model boundary is fixed, implemented after Litematica.
[[nodiscard]] ReadResult readSchem(const std::vector<std::uint8_t>& fileBytes);

// Reads a Bedrock structure file (.mcstructure: RAW uncompressed
// little-endian NBT, format_version == 1).
// OPEN QUESTION (do not freeze the decoder on assumption): flat-index axis
// order of block_indices is documented inconsistently (z-fastest per
// wiki.bedrock.dev/gist/crate vs y-fastest implied by one editor page).
// Resolve with a 2x2x2 empirical file before implementing. See docs.
[[nodiscard]] ReadResult readMcstructure(const std::vector<std::uint8_t>& fileBytes);

} // namespace architect::schematic
