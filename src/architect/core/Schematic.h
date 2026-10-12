#pragma once

#include "architect/core/Vec3i.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace architect {

// Canonical internal representation of one block state.
//
// `name` is the source-namespace identifier as stored in the file
// (e.g. "minecraft:oak_stairs" from a Java schematic, or "minecraft:stone"
// from an .mcstructure). Translation to the runtime Bedrock identifier
// happens in the mapping layer, never here, so readers stay lossless.
struct BlockState {
    std::string name;                            // e.g. "minecraft:stone"
    std::map<std::string, std::string> states;   // property name -> value, all as strings
    bool isAir = false;

    bool operator==(const BlockState&) const = default;
};

// Tile entity attached to one region-local cell (sign text, chest,
// furnace progress, ...). Only id + position are canonical in M2; full
// per-type NBT comparison is deferred (verifier limitation, documented).
struct TileEntity {
    std::string id; // e.g. "minecraft:chest"
    Vec3i pos{0, 0, 0}; // region-local cell

    bool operator==(const TileEntity&) const = default;
};

// One named cuboid region of a schematic (Litematica's unit of storage).
// Cells are stored in the file's native index order:
//
//   litematic: index = y * (sizeX * sizeZ) + z * sizeX + x   (x fastest)
//   schem v3:  index = x + z * sizeX + y * sizeX * sizeZ      (x fastest, y slowest)
//
// `cells` holds a palette index per cell in that order; `palette` maps the
// index to the decoded state. Palette id 0 should be air but readers must
// not assume it: treat whatever the palette says as authoritative.
struct Region {
    std::string name;
    Vec3i size{0, 0, 0};              // Extents in cells; Litematica Size may be signed in-file.
    Vec3i position{0, 0, 0};          // Region offset within the schematic (Litematica Position).
    std::vector<BlockState> palette;  // palette[0] is conventionally air.
    std::vector<std::uint32_t> cells; // size.x*size.y*size.z palette ids, native order.
    std::vector<TileEntity> tileEntities; // region-local tile entities (id + cell).
};

// Whole schematic: one or more regions plus file metadata.
struct Schematic {
    struct Metadata {
        std::string author;
        std::string description;
        std::string sourceFormat; // "litematic" | "schem" | "mcstructure"
        int formatVersion = 0;    // litematic Version / schem version / mcstructure format_version
        std::int64_t dataVersion = 0; // Java DataVersion where present; 0 = absent/unknown
        std::int64_t timeCreated = 0;
        std::int64_t timeModified = 0;
    };

    Metadata metadata;
    std::vector<Region> regions;

    // Total enclosed volume across all regions (sum of region volumes).
    [[nodiscard]] std::uint64_t enclosedVolume() const;
};

} // namespace architect
