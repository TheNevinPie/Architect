#include "architect/render/RenderSnapshot.h"

namespace architect::render {

Color colorFor(world::CellStatus status) {
    switch (status) {
        case world::CellStatus::Missing:
            return Color{1.0F, 0.25F, 0.25F, 0.9F};
        case world::CellStatus::Correct:
            return Color{0.25F, 1.0F, 0.35F, 0.9F};
        case world::CellStatus::Incorrect:
            return Color{1.0F, 0.65F, 0.15F, 0.9F};
        case world::CellStatus::Extra:
            return Color{0.75F, 0.35F, 1.0F, 0.9F};
        case world::CellStatus::Unchecked:
            return Color{0.6F, 0.6F, 0.6F, 0.9F};
    }
    return Color{};
}

} // namespace architect::render
