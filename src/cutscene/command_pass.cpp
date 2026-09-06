#include "off/cutscene/command_pass.hpp"

#include <bit>
#include <cmath>
#include <stdexcept>

namespace off::cutscene {
namespace {
std::int32_t checked_sentinel(float value) {
    if (!std::isfinite(value) || value < -0x1p31F || value >= 0x1p31F) {
        throw std::runtime_error("cut command sentinel is outside the supported signed range");
    }
    return static_cast<std::int32_t>(value);
}
std::int32_t schedule(const data::GmsIntroCutCommandSource& command) {
    return std::bit_cast<std::int32_t>(command.timeline_position);
}
}

CommandPass::CommandPass(std::span<const data::GmsIntroCutCommandSource> registration_order,
                         float derived_end)
    : commands_(registration_order.begin(), registration_order.end()) {
    if (!std::isfinite(derived_end)) throw std::runtime_error("cut derived end must be finite");
    const float empty = derived_end + 100000.0F;
    const float exhausted = derived_end + 1000000.0F;
    empty_sentinel_ = checked_sentinel(empty);
    exhausted_sentinel_ = checked_sentinel(exhausted);
    std::optional<std::size_t> cached;
    for (std::size_t input = 0; input < commands_.size(); ++input) {
        if (schedule(commands_[input]) < 0) continue;
        const float key = static_cast<float>(schedule(commands_[input]));
        std::size_t insertion = 0;
        if (cached) {
            insertion = *cached;
            while (static_cast<float>(schedule(commands_[order_[insertion]])) > key) {
                if (insertion == 0U) break;
                --insertion;
            }
            while (insertion < order_.size() &&
                   key > static_cast<float>(schedule(commands_[order_[insertion]]))) ++insertion;
        }
        order_.insert(order_.begin() + static_cast<std::ptrdiff_t>(insertion), input);
        cached = insertion;
    }
}

void CommandPass::reset_start() {
    if (running_) throw std::runtime_error("cut command phase cannot reset during a callback");
    cursor_.reset();
    next_position_ = 0;
}

void CommandPass::run(float position, const Visitor& visitor) {
    if (running_ || !visitor || !std::isfinite(position) ||
        position >= static_cast<float>(empty_sentinel_)) {
        throw std::runtime_error("cut command phase input or reentrancy is unsupported");
    }
    struct Guard {
        bool& flag;
        explicit Guard(bool& value) : flag(value) { flag = true; }
        ~Guard() { flag = false; }
    } guard(running_);
    while (position > static_cast<float>(next_position_)) {
        if (!cursor_) {
            next_position_ = empty_sentinel_;
            if (!order_.empty()) {
                cursor_ = 0U;
                next_position_ = schedule(commands_[order_[0]]);
            }
            continue;
        }
        const auto original_index = order_[*cursor_];
        visitor(commands_[original_index], original_index);
        next_position_ = exhausted_sentinel_;
        ++*cursor_;
        if (*cursor_ == order_.size()) cursor_.reset();
        else next_position_ = schedule(commands_[order_[*cursor_]]);
    }
}

} // namespace off::cutscene
