#include "off/cutscene/picture_fade.hpp"

#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace off::cutscene {
namespace {
struct Guard {
    bool& flag;
    explicit Guard(bool& value) : flag(value) {
        if (flag) throw std::runtime_error("picture fade reentrancy is unsupported");
        flag = true;
    }
    ~Guard() { flag = false; }
};
std::int32_t deadline_for(std::uint32_t raw, std::int32_t clock) {
    const auto argument = std::bit_cast<std::int32_t>(raw);
    if (argument <= 0) throw std::runtime_error("picture fade duration must be positive");
    const float seconds = static_cast<float>(argument) * 0.001F;
    const float negative_ticks = seconds * -1024.0F;
    if (!std::isfinite(negative_ticks) || negative_ticks < -0x1p31F || negative_ticks >= 0.0F)
        throw std::runtime_error("picture fade duration conversion is unsupported");
    const auto delta = static_cast<std::int32_t>(negative_ticks);
    const auto duration = -static_cast<std::int64_t>(delta);
    const auto end = static_cast<std::int64_t>(clock) + duration;
    if (duration <= 0 || duration > std::numeric_limits<std::int32_t>::max() ||
        end > std::numeric_limits<std::int32_t>::max())
        throw std::runtime_error("picture fade deadline or duration overflows");
    return static_cast<std::int32_t>(end);
}
}

void PictureFade::event(std::string_view name, std::uint32_t argument, std::int32_t clock,
                        const Visitor& visitor) {
    Guard guard(running_);
    if (name != "FadeIn" && name != "FadeOut") return;
    if (!visitor) throw std::runtime_error("picture fade visitor is required");
    const auto end = argument == 0 ? clock : deadline_for(argument, clock);
    if (name == "FadeIn") {
        if (argument == 0) {
            visitor({EffectKind::owner_control, 1});
            state_ = State::idle_clear;
            return;
        }
    } else {
        visitor({EffectKind::owner_control, 0});
        visitor({EffectKind::alpha, static_cast<std::uint8_t>(argument == 0 ? 254 : 1)});
        if (argument == 0) {
            state_ = State::idle_covered;
            return;
        }
    }
    start_ = clock;
    deadline_ = end;
    state_ = name == "FadeIn" ? State::fading_in : State::fading_out;
}

void PictureFade::update(std::int32_t clock, const Visitor& visitor) {
    Guard guard(running_);
    if (state_ != State::fading_in && state_ != State::fading_out) return;
    if (!visitor || clock < start_)
        throw std::runtime_error("picture fade update input is unsupported");
    const bool inward = state_ == State::fading_in;
    if (clock > deadline_) {
        visitor({EffectKind::alpha, static_cast<std::uint8_t>(inward ? 0 : 254)});
        if (inward) visitor({EffectKind::owner_control, 1});
        state_ = inward ? State::idle_clear : State::idle_covered;
        return;
    }
    auto fraction = ((static_cast<std::int64_t>(clock) - start_) * 1024) /
                    (static_cast<std::int64_t>(deadline_) - start_);
    if (inward) fraction = 1024 - fraction;
    visitor({EffectKind::alpha, static_cast<std::uint8_t>((254 * fraction) / 1024)});
}

} // namespace off::cutscene
