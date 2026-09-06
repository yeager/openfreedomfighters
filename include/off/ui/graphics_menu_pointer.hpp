#pragma once

#include "off/ui/graphics_menu_draw.hpp"
#include <array>
#include <optional>

namespace off::ui {

[[nodiscard]] std::optional<std::array<float, 2>>
map_graphics_menu_pointer_to_pixels(float x, float y, UiExtent logical,
                                    UiExtent physical);

// Generates fresh targets from the current menu phase and physical viewport.
// Project policy: option clicks focus and advance via Right; action clicks use
// Enter. Confirmation exposes only Keep/Revert. No stale caller-provided list
// can bypass phase, disabled-target or viewport checks.
[[nodiscard]] GraphicsMenuEffect dispatch_graphics_menu_pointer(
    GraphicsMenuSession &menu, UiExtent physical_target, float x, float y,
    GraphicsClock::time_point now, float ui_scale = 1.0F);

} // namespace off::ui
