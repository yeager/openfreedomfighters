#pragma once

#include <optional>
#include <string_view>

namespace off {

enum class Mode { original, modern };

[[nodiscard]] constexpr std::optional<Mode> parse_mode(std::string_view value) {
    if (value == "original") {
        return Mode::original;
    }
    if (value == "modern") {
        return Mode::modern;
    }
    return std::nullopt;
}

[[nodiscard]] constexpr std::string_view mode_name(Mode mode) {
    return mode == Mode::original ? "Original" : "Modern";
}

}  // namespace off

