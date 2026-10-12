// M2: readLitematicFromRoot over a programmatically built root.
// Mirrors the fixture shape (3x2x4 region, 7-state palette) plus every
// rejection in the contract. stdlib only.
#include "architect/schematic/Readers.h"

#include <cstdio>

namespace {

int failures = 0;

void check(const char* name, bool cond) {
    std::printf("%s %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) {
        ++failures;
    }
}

using namespace architect;
using namespace architect::nbt;

Tag str(const std::string& s) {
    Tag t;
    t.type  = TagType::String;
    t.value = s;
    return t;
}
Tag i32(std::int32_t v) {
    Tag t;
    t.type  = TagType::Int;
    t.value = v;
    return t;
}
Tag longs(std::vector<std::int64_t> v) {
    Tag t;
    t.type  = TagType::LongArray;
    t.value = std::move(v);
    return t;
}
Compound xyz(int x, int y, int z) {
    return Compound{{"x", i32(x)}, {"y", i32(y)}, {"z", i32(z)}};
}
Tag comp(Compound c) {
    Tag t;
    t.type  = TagType::Compound;
    t.value = std::move(c);
    return t;
}

// Builds the M1 fixture root: region "test", size 3x2x4, palette of 7,
// cells with idx0=stone(1), idx21=oak_log(4), idx23=wall_torch(5),
// one chest tile entity. Packs BlockStates with the same LSB-first
// algorithm as core/Bits.h (independent reimplementation here).
Compound fixtureRoot(int version, bool shortArray, bool badId) {
    const std::vector<std::pair<std::string, std::map<std::string, std::string>>> pal = {
        {"minecraft:air", {}},
        {"minecraft:stone", {}},
        {"minecraft:oak_stairs", {{"facing", "east"}, {"half", "bottom"}}},
        {"minecraft:glass", {}},
        {"minecraft:oak_log", {{"axis", "y"}}},
        {"minecraft:wall_torch", {{"facing", "north"}}},
        {"minecraft:chest", {{"facing", "south"}}},
    };
    List palette;
    for (const auto& [name, props] : pal) {
        Compound e;
        e["Name"] = str(name);
        if (!props.empty()) {
            Compound p;
            for (const auto& [k, v] : props) {
                p[k] = str(v);
            }
            e["Properties"] = comp(std::move(p));
        }
        palette.push_back(comp(std::move(e)));
    }
    std::vector<std::uint32_t> ids(24, 0);
    ids[0]  = badId ? 7 : 1; // 7 is in-range for 3-bit storage but out-of-range for a 7-entry palette
    ids[4]  = 2;
    ids[9]  = 6;
    ids[19] = 1;
    ids[21] = 4;
    ids[23] = 5;
    std::vector<std::uint64_t> raw(2, 0);
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const std::uint64_t s = i * 3;
        raw[s / 64] |= static_cast<std::uint64_t>(ids[i]) << (s % 64);
        if (s % 64 + 3 > 64) {
            raw[s / 64 + 1] |= static_cast<std::uint64_t>(ids[i]) >> (64 - s % 64);
        }
    }
    std::vector<std::int64_t> words = {static_cast<std::int64_t>(raw[0]), static_cast<std::int64_t>(raw[1])};
    if (shortArray) {
        words.pop_back();
    }
    List tents;
    Compound tent;
    tent["id"] = str("minecraft:chest");
    tent["x"]  = i32(0);
    tent["y"]  = i32(0);
    tent["z"]  = i32(3);
    tents.push_back(comp(std::move(tent)));

    Compound region;
    region["Position"]          = comp(xyz(10, 64, -5));
    region["Size"]              = comp(xyz(3, 2, 4));
    Tag plt;
    plt.type  = TagType::List;
    plt.value = std::move(palette);
    region["BlockStatePalette"] = std::move(plt);
    region["BlockStates"]       = longs(std::move(words));
    Tag te;
    te.type  = TagType::List;
    te.value = std::move(tents);
    region["TileEntities"] = std::move(te);

    Compound root;
    root["Version"]              = i32(version);
    root["SubVersion"]           = i32(1);
    root["MinecraftDataVersion"] = i32(4189);
    Compound regions;
    regions["test"] = comp(std::move(region));
    root["Regions"] = comp(std::move(regions));
    return root;
}

} // namespace

int testReader() {
    using namespace architect::schematic;

    ReadResult ok = readLitematicFromRoot(fixtureRoot(7, false, false));
    check("fixture root decodes", std::holds_alternative<Schematic>(ok));
    if (std::holds_alternative<Schematic>(ok)) {
        const Schematic& s = std::get<Schematic>(ok);
        check("one region", s.regions.size() == 1);
        check("region name", s.regions[0].name == "test");
        check("region size", s.regions[0].size == Vec3i{3, 2, 4});
        check("region pos", s.regions[0].position == Vec3i{10, 64, -5});
        check("24 cells", s.regions[0].cells.size() == 24);
        check("idx0 stone", s.regions[0].cells[0] == 1);
        check("straddle idx21 oak_log", s.regions[0].cells[21] == 4);
        check("idx23 torch", s.regions[0].cells[23] == 5);
        check("stairs props kept", s.regions[0].palette[2].states.at("facing") == "east");
        check("air flagged", s.regions[0].palette[0].isAir);
        check("tile entity kept", s.regions[0].tileEntities.size() == 1);
        check(
            "tile entity id+pos",
            s.regions[0].tileEntities[0].id == "minecraft:chest" &&
                s.regions[0].tileEntities[0].pos == Vec3i{0, 0, 3}
        );
        check("format metadata", s.metadata.sourceFormat == "litematic" && s.metadata.formatVersion == 7);
        check("data version", s.metadata.dataVersion == 4189);
    }

    auto expectReject = [](const char* name, Compound root) {
        ReadResult r = readLitematicFromRoot(root);
        const auto* err = std::get_if<ReadError>(&r);
        check(name, err != nullptr);
    };
    expectReject("Version 1 rejected", fixtureRoot(1, false, false));
    expectReject("Version 8 rejected", fixtureRoot(8, false, false));
    expectReject("short BlockStates rejected", fixtureRoot(7, true, false));
    expectReject("out-of-range id rejected", fixtureRoot(7, false, true));
    expectReject("missing Regions rejected", Compound{{"Version", i32(7)}});

    // Signed Size{-2,1,1} normalizes to {2,1,1} (2 cells, 2-bit entries).
    {
        List palette;
        Compound air;
        air["Name"] = str("minecraft:air");
        Compound dirt;
        dirt["Name"] = str("minecraft:dirt");
        palette.push_back(comp(std::move(air)));
        palette.push_back(comp(std::move(dirt)));
        Compound region;
        region["Position"] = comp(xyz(0, 0, 0));
        region["Size"]     = comp(xyz(-2, 1, 1));
        Tag plt;
        plt.type  = TagType::List;
        plt.value = std::move(palette);
        region["BlockStatePalette"] = std::move(plt);
        region["BlockStates"]       = longs({static_cast<std::int64_t>(0b0100ULL)});
        Compound regions;
        regions["neg"] = comp(std::move(region));
        Compound root;
        root["Version"] = i32(7);
        root["Regions"] = comp(std::move(regions));
        ReadResult r = readLitematicFromRoot(root);
        check("negative size decodes", std::holds_alternative<Schematic>(r));
        if (std::holds_alternative<Schematic>(r)) {
            const Schematic& s = std::get<Schematic>(r);
            check("size abs-normalized", s.regions[0].size == Vec3i{2, 1, 1});
            check(
                "cells [air,dirt]",
                s.regions[0].cells.size() == 2 && s.regions[0].cells[0] == 0 && s.regions[0].cells[1] == 1
            );
        }
    }
    return failures;
}
