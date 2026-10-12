// M2: Litematica decoder implemented over the M2 NBT backend.
// Tag layout ground truth: litemapy/schematic.py (master) — Region tags
// Position/Size/BlockStatePalette/BlockStates/Entities/TileEntities/
// PendingBlockTicks/PendingFluidTicks; root Version/SubVersion/
// MinecraftDataVersion/Metadata/EnclosingSize/Author/Description/Name/
// RegionCount/TimeCreated/TimeModified/TotalBlocks/TotalVolume, Regions{}.
#include "architect/schematic/Readers.h"

#include "architect/core/Bits.h"

#include <cmath>
#include <cstdint>

namespace architect::schematic {
namespace {

const nbt::Tag* required(const nbt::Compound& c, const char* key, nbt::TagType type, std::string& err) {
    const auto t = nbt::find(c, key);
    if (!t || (*t)->type != type) {
        err = std::string("missing or mistyped tag '") + key + "'";
        return nullptr;
    }
    return *t;
}

// Unwrap helpers: asX returns optional<pointer>; these turn a present
// value into a raw pointer or record an error (never dereference nullopt).
const nbt::Compound* compoundOf(const nbt::Tag& tag, const char* what, std::string& err) {
    const auto opt = nbt::asCompound(tag);
    if (!opt) {
        err = std::string(what) + " is not a compound";
        return nullptr;
    }
    return *opt;
}

const std::string* stringOf(const nbt::Tag& tag, const char* what, std::string& err) {
    const auto opt = nbt::asString(tag);
    if (!opt) {
        err = std::string(what) + " is not a string";
        return nullptr;
    }
    return *opt;
}

bool getInt(const nbt::Tag& tag, std::int64_t& out) {
    // Size/Position fields are Int; accept any integer width defensively.
    if (const auto v = nbt::asInt(tag)) {
        out = *v;
        return true;
    }
    if (const auto v = nbt::asShort(tag)) {
        out = *v;
        return true;
    }
    if (const auto v = nbt::asByte(tag)) {
        out = *v;
        return true;
    }
    if (const auto v = nbt::asLong(tag)) {
        out = *v;
        return true;
    }
    return false;
}

bool getVec3(const nbt::Compound& c, const char* key, Vec3i& out, std::string& err) {
    const nbt::Tag* tag = required(c, key, nbt::TagType::Compound, err);
    if (!tag) {
        return false;
    }
    const nbt::Compound* comp = compoundOf(*tag, key, err);
    if (!comp) {
        return false;
    }
    std::int64_t x = 0, y = 0, z = 0;
    const auto tx = nbt::find(*comp, "x");
    const auto ty = nbt::find(*comp, "y");
    const auto tz = nbt::find(*comp, "z");
    if (!tx || !ty || !tz || !getInt(**tx, x) || !getInt(**ty, y) || !getInt(**tz, z)) {
        err = std::string("bad '") + key + "' coordinates";
        return false;
    }
    out = Vec3i{static_cast<int>(x), static_cast<int>(y), static_cast<int>(z)};
    return true;
}

bool decodePaletteEntry(const nbt::Tag& tag, BlockState& out, std::string& err) {
    const nbt::Compound* comp = compoundOf(tag, "palette entry", err);
    if (!comp) {
        return false;
    }
    const nbt::Tag* name = required(*comp, "Name", nbt::TagType::String, err);
    if (!name) {
        return false;
    }
    const std::string* nameStr = stringOf(*name, "palette 'Name'", err);
    if (!nameStr) {
        return false;
    }
    out.name = *nameStr;
    out.states.clear();
    if (const auto props = nbt::find(*comp, "Properties")) {
        const nbt::Compound* pcomp = compoundOf(**props, "palette 'Properties'", err);
        if (!pcomp) {
            return false;
        }
        for (const auto& [key, value] : *pcomp) {
            const std::string* s = stringOf(value, "palette property", err);
            if (!s) {
                return false;
            }
            out.states[key] = *s;
        }
    }
    out.isAir = (out.name == "minecraft:air");
    return true;
}

} // namespace

ReadResult readLitematicFromRoot(const nbt::Compound& root) {
    std::string err;
    const nbt::Tag* versionTag = required(root, "Version", nbt::TagType::Int, err);
    if (!versionTag) {
        return ReadError{err};
    }
    const int version = *nbt::asInt(*versionTag);
    if (version == 1 || version > 7) {
        return ReadError{"unsupported litematic Version " + std::to_string(version) + " (accept 2..7)"};
    }

    Schematic schematic;
    schematic.metadata.sourceFormat = "litematic";
    schematic.metadata.formatVersion = version;
    if (const auto t = nbt::find(root, "SubVersion"); t && (*t)->type == nbt::TagType::Int) {
        // SubVersion carried in formatVersion lineage is not separately stored in M2;
        // unknown future sub-versions parse normally (warn-at-UI-layer policy).
    }
    if (const auto t = nbt::find(root, "MinecraftDataVersion"); t) {
        std::int64_t dv = 0;
        if (getInt(**t, dv)) {
            schematic.metadata.dataVersion = dv;
        }
    }
    if (const auto t = nbt::find(root, "Metadata"); t && (*t)->type == nbt::TagType::Compound) {
        std::string metaErr;
        const nbt::Compound* meta = compoundOf(**t, "Metadata", metaErr);
        if (meta) {
            const auto getStr = [&](const char* key, std::string& out) {
                if (const auto f = nbt::find(*meta, key); f && (*f)->type == nbt::TagType::String) {
                    std::string s;
                    if (const std::string* v = stringOf(**f, key, s)) {
                        out = *v;
                    }
                }
            };
            getStr("Author", schematic.metadata.author);
            getStr("Description", schematic.metadata.description);
            const auto getI64 = [&](const char* key, std::int64_t& out) {
                if (const auto f = nbt::find(*meta, key); f) {
                    std::int64_t v = 0;
                    if (getInt(**f, v)) {
                        out = v;
                    }
                }
            };
            getI64("TimeCreated", schematic.metadata.timeCreated);
            getI64("TimeModified", schematic.metadata.timeModified);
        }
    }

    const nbt::Tag* regionsTag = required(root, "Regions", nbt::TagType::Compound, err);
    if (!regionsTag) {
        return ReadError{err};
    }
    const nbt::Compound* regions = compoundOf(*regionsTag, "Regions", err);
    if (!regions) {
        return ReadError{err};
    }
    for (const auto& [regionName, regionTag] : *regions) {
        if (regionTag.type != nbt::TagType::Compound) {
            return ReadError{"region '" + regionName + "' is not a compound"};
        }
        std::string rcErr;
        const nbt::Compound* rc = compoundOf(regionTag, "region", rcErr);
        if (!rc) {
            return ReadError{"region '" + regionName + "': " + rcErr};
        }
        Region region;
        region.name = regionName;
        Vec3i pos{0, 0, 0};
        Vec3i size{0, 0, 0};
        if (!getVec3(*rc, "Position", pos, err)) {
            return ReadError{"region '" + regionName + "': " + err};
        }
        if (!getVec3(*rc, "Size", size, err)) {
            return ReadError{"region '" + regionName + "': " + err};
        }
        region.position = pos;
        // Signed Size normalized with abs() (litemapy allows negative extents).
        region.size = Vec3i{
            size.x < 0 ? -size.x : size.x,
            size.y < 0 ? -size.y : size.y,
            size.z < 0 ? -size.z : size.z,
        };
        if (region.size.x <= 0 || region.size.y <= 0 || region.size.z <= 0) {
            return ReadError{"region '" + regionName + "' has a zero extent"};
        }
        const std::uint64_t volume = static_cast<std::uint64_t>(region.size.x) *
                                     static_cast<std::uint64_t>(region.size.y) *
                                     static_cast<std::uint64_t>(region.size.z);

        const nbt::Tag* paletteTag = required(*rc, "BlockStatePalette", nbt::TagType::List, err);
        if (!paletteTag) {
            return ReadError{"region '" + regionName + "': " + err};
        }
        const auto paletteOpt = nbt::asList(*paletteTag);
        if (!paletteOpt) {
            return ReadError{"region '" + regionName + "': palette is not a list"};
        }
        const nbt::List* paletteList = *paletteOpt;
        if (paletteList->empty()) {
            return ReadError{"region '" + regionName + "': empty palette"};
        }
        for (const auto& entry : *paletteList) {
            BlockState state;
            if (!decodePaletteEntry(entry, state, err)) {
                return ReadError{"region '" + regionName + "': " + err};
            }
            region.palette.push_back(std::move(state));
        }

        const auto bits = detail::bitsPerEntry(region.palette.size());
        if (!bits) {
            return ReadError{"region '" + regionName + "': empty palette"};
        }
        const nbt::Tag* statesTag = required(*rc, "BlockStates", nbt::TagType::LongArray, err);
        if (!statesTag) {
            return ReadError{"region '" + regionName + "': " + err};
        }
        const auto wordsOpt = nbt::asLongArray(*statesTag);
        if (!wordsOpt) {
            return ReadError{"region '" + regionName + "': BlockStates is not a long array"};
        }
        const std::vector<std::int64_t>* words = *wordsOpt;
        if (words->size() != detail::packedLongCount(volume, *bits)) {
            return ReadError{"region '" + regionName + "': BlockStates length mismatch"};
        }
        region.cells.reserve(static_cast<std::size_t>(volume));
        for (std::uint64_t i = 0; i < volume; ++i) {
            const auto id = detail::unpackEntry(*words, i, *bits, region.palette.size());
            if (!id) {
                return ReadError{"region '" + regionName + "': palette id out of range at index " + std::to_string(i)};
            }
            region.cells.push_back(*id);
        }

        if (const auto t = nbt::find(*rc, "TileEntities"); t && (*t)->type == nbt::TagType::List) {
            const auto tentsOpt = nbt::asList(**t);
            if (!tentsOpt) {
                return ReadError{"region '" + regionName + "': TileEntities is not a list"};
            }
            for (const auto& tent : **tentsOpt) {
                std::string tentErr;
                const nbt::Compound* tc = compoundOf(tent, "tile entity", tentErr);
                if (!tc) {
                    continue; // Best-effort: a malformed tile entity never fails the blocks.
                }
                TileEntity te;
                if (const auto f = nbt::find(*tc, "id"); f && (*f)->type == nbt::TagType::String) {
                    std::string idErr;
                    if (const std::string* id = stringOf(**f, "tile entity id", idErr)) {
                        te.id = *id;
                    }
                }
                std::int64_t x = 0, y = 0, z = 0;
                if (const auto fx = nbt::find(*tc, "x")) {
                    getInt(**fx, x);
                }
                if (const auto fy = nbt::find(*tc, "y")) {
                    getInt(**fy, y);
                }
                if (const auto fz = nbt::find(*tc, "z")) {
                    getInt(**fz, z);
                }
                te.pos = Vec3i{static_cast<int>(x), static_cast<int>(y), static_cast<int>(z)};
                region.tileEntities.push_back(std::move(te));
            }
        }
        schematic.regions.push_back(std::move(region));
    }
    if (schematic.regions.empty()) {
        return ReadError{"schematic has no regions"};
    }
    return schematic;
}

ReadResult readLitematic(const std::vector<std::uint8_t>& fileBytes) {
    // .litematic is gzip + big-endian NBT: inflate, parse, decode.
    const auto parsed = nbt::readRootCompound(fileBytes, nbt::ByteOrder::Big, true);
    if (const auto* err = std::get_if<nbt::ParseError>(&parsed)) {
        return ReadError{"NBT parse failed: " + err->message};
    }
    return readLitematicFromRoot(std::get<nbt::Compound>(parsed));
}

ReadResult readSchem(const std::vector<std::uint8_t>&) {
    return ReadError{"M2: Sponge .schem decoder not implemented (deferred past litematic)"};
}

ReadResult readMcstructure(const std::vector<std::uint8_t>&) {
    return ReadError{"M2: .mcstructure decoder blocked on axis-order question (see Readers.h)"};
}

} // namespace architect::schematic
