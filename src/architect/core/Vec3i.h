#pragma once

#include <cstdint>
#include <functional>

namespace architect {

// Integer 3D coordinate. Used for region-local cells, sizes, and world
// anchors. Pure value type: no engine dependency.
struct Vec3i {
    int x = 0;
    int y = 0;
    int z = 0;

    constexpr bool operator==(const Vec3i&) const = default;
};

struct Vec3iHash {
    std::size_t operator()(const Vec3i& v) const noexcept {
        // Simple FNV-1a mix over the three lanes.
        std::size_t h = 1469598103934665603ULL;
        for (int lane : {v.x, v.y, v.z}) {
            h ^= static_cast<std::uint32_t>(lane);
            h *= 1099511628211ULL;
        }
        return h;
    }
};

} // namespace architect
