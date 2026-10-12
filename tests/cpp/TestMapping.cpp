// M2: mapping policy cases (hand-computed). stdlib only.
#include "architect/mapping/BlockMap.h"

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

int testMapping() {
    using namespace architect;
    using namespace architect::mapping;

    BlockState air{"minecraft:air", {}, true};
    MappedBlock m = mapJavaToBedrock(air);
    check("air exact", m.kind == MapResultKind::Exact && m.bedrockName == "minecraft:air");

    BlockState stone{"minecraft:stone", {}, false};
    m = mapJavaToBedrock(stone);
    check("stone exact passthrough", m.kind == MapResultKind::Exact && m.bedrockName == "minecraft:stone");

    BlockState stairs{"minecraft:oak_stairs", {{"facing", "east"}, {"half", "bottom"}, {"shape", "straight"}}, false};
    m = mapJavaToBedrock(stairs);
    check(
        "stairs exact (allowlisted states)",
        m.kind == MapResultKind::Exact && m.bedrockStates.at("facing") == "east" &&
            m.bedrockStates.at("shape") == "straight"
    );

    BlockState waterStairs = stairs;
    waterStairs.states["waterlogged"] = "true";
    m = mapJavaToBedrock(waterStairs);
    check(
        "waterlogged partial + layer note",
        m.kind == MapResultKind::Partial && m.note.find("second layer") != std::string::npos &&
            m.bedrockStates.find("waterlogged") == m.bedrockStates.end()
    );

    BlockState odd{"minecraft:oak_stairs", {{"facing", "east"}, {"crackLevel", "3"}}, false};
    m = mapJavaToBedrock(odd);
    check(
        "unknown property partial + named",
        m.kind == MapResultKind::Partial && m.note.find("crackLevel") != std::string::npos
    );

    BlockState modded{"biomeswevegone:ebony_planks", {}, false};
    m = mapJavaToBedrock(modded);
    check(
        "non-vanilla placeholder keeps name",
        m.kind == MapResultKind::Placeholder && m.bedrockName == "biomeswevegone:ebony_planks"
    );

    BlockState empty{"", {}, false};
    m = mapJavaToBedrock(empty);
    check("empty name placeholder", m.kind == MapResultKind::Placeholder);

    // Fixture palette maps without loss except the chest waterlogged=false
    // (structural, dropped -> Partial) — matches generator data.
    BlockState chest{
        "minecraft:chest", {{"facing", "south"}, {"type", "single"}, {"waterlogged", "false"}}, false};
    m = mapJavaToBedrock(chest);
    check("chest partial (waterlogged key dropped)", m.kind == MapResultKind::Partial);

    BlockState log{"minecraft:oak_log", {{"axis", "y"}}, false};
    m = mapJavaToBedrock(log);
    check("log exact", m.kind == MapResultKind::Exact && m.bedrockStates.at("axis") == "y");
    return failures;
}
