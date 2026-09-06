#pragma once

#include "off/data/gms_image.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace off::cutscene {

// A conditional, already-admitted command phase, not a cut-sequence player.
// Registration order is explicit; callbacks must not destroy this owner.
class CommandPass final {
public:
    using Visitor = std::function<void(const data::GmsIntroCutCommandSource&, std::size_t)>;
    CommandPass(std::span<const data::GmsIntroCutCommandSource> registration_order, float derived_end);
    CommandPass(const CommandPass&) = delete;
    CommandPass& operator=(const CommandPass&) = delete;
    CommandPass(CommandPass&&) = delete;
    CommandPass& operator=(CommandPass&&) = delete;

    // Rejects reentrancy and positions at/above the empty-list sentinel as
    // explicit native safety policies. Callback exceptions retain the current
    // command for retry; earlier callbacks are not rolled back.
    void run(float position, const Visitor& visitor);
    void reset_start();

private:
    std::vector<data::GmsIntroCutCommandSource> commands_;
    std::vector<std::size_t> order_;
    std::optional<std::size_t> cursor_;
    std::int32_t next_position_{0};
    std::int32_t empty_sentinel_{0};
    std::int32_t exhausted_sentinel_{0};
    bool running_{false};
};

} // namespace off::cutscene
