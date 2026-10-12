#include "architect/ui/Controls.h"

namespace architect::ui {

std::vector<Binding> defaultBindings() {
    return {
        {Action::ToggleOverlay, "architect.toggle_overlay", "K", "Show/hide the schematic blueprint overlay"},
        {Action::MoveAnchorXPlus, "architect.anchor_x_plus", "Right", "Nudge schematic anchor +X"},
        {Action::MoveAnchorXMinus, "architect.anchor_x_minus", "Left", "Nudge schematic anchor -X"},
        {Action::MoveAnchorZPlus, "architect.anchor_z_plus", "Down", "Nudge schematic anchor +Z"},
        {Action::MoveAnchorZMinus, "architect.anchor_z_minus", "Up", "Nudge schematic anchor -Z"},
        {Action::MoveAnchorYPlus, "architect.anchor_y_plus", "PageUp", "Nudge schematic anchor +Y"},
        {Action::MoveAnchorYMinus, "architect.anchor_y_minus", "PageDown", "Nudge schematic anchor -Y"},
        {Action::RotateCw, "architect.rotate_cw", "R", "Rotate schematic 90 deg clockwise"},
        {Action::CycleLayerFilter, "architect.cycle_layer", "L", "Cycle the rendered Y-layer filter"},
        {Action::ShowMaterialCounts, "architect.material_counts", "M", "Print missing-material counts to chat"},
    };
}

} // namespace architect::ui
