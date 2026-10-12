#include "architect/core/Schematic.h"

namespace architect {

std::uint64_t Schematic::enclosedVolume() const {
    std::uint64_t total = 0;
    for (const Region& region : regions) {
        total += static_cast<std::uint64_t>(region.size.x) * static_cast<std::uint64_t>(region.size.y) *
                 static_cast<std::uint64_t>(region.size.z);
    }
    return total;
}

} // namespace architect
