#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace architect::ui {

// User-facing actions. The core only assigns stable ids + default bindings;
// the game adapter wires them to ll::input::KeyRegistry and chat/HUD output.
// No engine include here: keeps the core SDK-free.
enum class Action : std::uint8_t {
    ToggleOverlay = 0,
    MoveAnchorXPlus,
    MoveAnchorXMinus,
    MoveAnchorZPlus,
    MoveAnchorZMinus,
    MoveAnchorYPlus,
    MoveAnchorYMinus,
    RotateCw,
    CycleLayerFilter,
    ShowMaterialCounts,
};

struct Binding {
    Action action = Action::ToggleOverlay;
    std::string name;        // KeyRegistry name, e.g. "architect.toggle_overlay"
    std::string defaultKey;  // human-readable default, e.g. "K"
    std::string description;
};

[[nodiscard]] std::vector<Binding> defaultBindings();

} // namespace architect::ui
