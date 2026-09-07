#pragma once

#include "off/cutscene/picture_fade.hpp"

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace off::cutscene {

// A live, admitted picture-owner receiver for the first cut only.  It has no
// authority to admit an owner, create a render picture, or start a cut.
class FirstCutFadeTargetComponent final {
public:
    using OwnerControl = std::function<void(bool)>;
    using SetAlpha = std::function<void(std::uint8_t)>;

    FirstCutFadeTargetComponent(std::uint64_t admitted_owner_identity,
                                std::uint16_t fade_in_event,
                                std::uint16_t fade_out_event,
                                OwnerControl owner_control, SetAlpha set_alpha)
        : owner_identity_(admitted_owner_identity), fade_in_event_(fade_in_event),
          fade_out_event_(fade_out_event), owner_control_(std::move(owner_control)),
          set_alpha_(std::move(set_alpha)) {
        if (owner_identity_ == 0 || fade_in_event_ == 0 || fade_out_event_ == 0 ||
            fade_in_event_ == fade_out_event_ || !owner_control_ || !set_alpha_)
            throw std::runtime_error("first cut fade target construction is unsupported");
    }
    FirstCutFadeTargetComponent(const FirstCutFadeTargetComponent&) = delete;
    FirstCutFadeTargetComponent& operator=(const FirstCutFadeTargetComponent&) = delete;
    FirstCutFadeTargetComponent(FirstCutFadeTargetComponent&&) = delete;
    FirstCutFadeTargetComponent& operator=(FirstCutFadeTargetComponent&&) = delete;

    void event(std::uint16_t event_id, std::uint32_t argument, std::int32_t clock) {
        if (event_id == fade_in_event_)
            fade_.event(PictureFade::Event::fade_in, argument, clock, visitor());
        else if (event_id == fade_out_event_)
            fade_.event(PictureFade::Event::fade_out, argument, clock, visitor());
    }
    void update(std::int32_t clock) { fade_.update(clock, visitor()); }

    [[nodiscard]] std::uint64_t owner_identity() const noexcept { return owner_identity_; }
    [[nodiscard]] std::uint16_t fade_in_event() const noexcept { return fade_in_event_; }
    [[nodiscard]] std::uint16_t fade_out_event() const noexcept { return fade_out_event_; }
    [[nodiscard]] PictureFade::State state() const noexcept { return fade_.state(); }

private:
    [[nodiscard]] PictureFade::Visitor visitor() {
        return [this](PictureFade::Effect effect) {
            if (effect.kind == PictureFade::EffectKind::owner_control)
                owner_control_(effect.value != 0);
            else
                set_alpha_(effect.value);
        };
    }

    std::uint64_t owner_identity_;
    std::uint16_t fade_in_event_;
    std::uint16_t fade_out_event_;
    OwnerControl owner_control_;
    SetAlpha set_alpha_;
    PictureFade fade_;
};

} // namespace off::cutscene
