#pragma once

#include <cstdint>
#include <functional>
#include <string_view>

namespace off::cutscene {

// Conditional receiver only: no initial owner color or render admission.
class PictureFade final {
public:
    enum class State { idle_clear, fading_in, fading_out, idle_covered };
    enum class EffectKind { owner_control, alpha };
    struct Effect {
        EffectKind kind;
        std::uint8_t value;
        bool operator==(const Effect&) const = default;
    };
    using Visitor = std::function<void(Effect)>;
    PictureFade() = default;
    PictureFade(const PictureFade&) = delete;
    PictureFade& operator=(const PictureFade&) = delete;
    PictureFade(PictureFade&&) = delete;
    PictureFade& operator=(PictureFade&&) = delete;

    // Fixed-clock policy requires an unchanged scene clock across callbacks and
    // the original separate timing reads. Unsupported inputs fail before effects.
    // Visitors must not reenter or destroy this object. Exceptions preserve
    // emitted prefix effects; state writes following the throw have not happened.
    void event(std::string_view name, std::uint32_t argument, std::int32_t clock,
               const Visitor& visitor);
    void update(std::int32_t clock, const Visitor& visitor);
    [[nodiscard]] State state() const noexcept { return state_; }

private:
    State state_{State::idle_covered};
    std::int32_t start_{0};
    std::int32_t deadline_{0};
    bool running_{false};
};

} // namespace off::cutscene
