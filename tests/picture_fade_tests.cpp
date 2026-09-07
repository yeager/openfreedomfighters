#include "off/cutscene/picture_fade.hpp"
#include "off/cutscene/first_cut_fade_target_component.hpp"

#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {
using Fade = off::cutscene::PictureFade;
using State = Fade::State;
using Effect = Fade::Effect;
using FirstCutFade = off::cutscene::FirstCutFadeTargetComponent;
constexpr auto control = Fade::EffectKind::owner_control;
constexpr auto alpha = Fade::EffectKind::alpha;
int failures = 0;
void check(bool value, const char* description) {
    if (!value) { ++failures; std::cerr << "FAIL: " << description << '\n'; }
}
template<class Operation> void rejects(Operation operation) {
    bool rejected = false;
    try { operation(); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "unsupported operation must throw");
}
}

int main() {
    static_assert(!std::is_copy_constructible_v<Fade> && !std::is_move_constructible_v<Fade>);
    static_assert(!std::is_copy_assignable_v<Fade> && !std::is_move_assignable_v<Fade>);
    Fade fade;
    std::vector<Effect> effects;
    const Fade::Visitor record = [&](Effect effect) { effects.push_back(effect); };
    check(fade.state() == State::idle_covered, "constructor state only, no invented owner output");
    fade.update(0, {});
    fade.event("Unknown", 0xffffffffU, std::numeric_limits<std::int32_t>::max(), {});
    check(effects.empty() && fade.state() == State::idle_covered, "idle and unknown do nothing");

    fade.event("FadeIn", 0, 100, record);
    check(effects == std::vector<Effect>{{control, 1}} && fade.state() == State::idle_clear,
          "instant fade in does not set alpha");
    effects.clear();
    fade.event("FadeOut", 0, 100, record);
    check(effects == std::vector<Effect>{{control, 0}, {alpha, 254}} &&
          fade.state() == State::idle_covered, "instant fade out has ordered effects and endpoint 254");

    effects.clear();
    fade.event("FadeOut", 125, 100, record); // Exactly 128 scene-clock units.
    check(effects == std::vector<Effect>{{control, 0}, {alpha, 1}} &&
          fade.state() == State::fading_out, "timed fade out writes alpha one immediately");
    effects.clear();
    fade.update(100, record); fade.update(164, record); fade.update(228, record);
    check(effects == std::vector<Effect>{{alpha, 0}, {alpha, 127}, {alpha, 254}} &&
          fade.state() == State::fading_out, "exact start, midpoint and deadline preserve transition");
    fade.update(229, record);
    check(effects.back() == Effect{alpha, 254} && fade.state() == State::idle_covered,
          "strictly later update completes fade out");
    effects.clear();
    fade.update(230, record); check(effects.empty(), "covered idle emits nothing");

    fade.event("FadeIn", 125, 300, record);
    check(effects.empty() && fade.state() == State::fading_in, "timed fade in has no immediate output");
    fade.update(300, record); fade.update(364, record); fade.update(428, record);
    check(effects == std::vector<Effect>{{alpha, 254}, {alpha, 127}, {alpha, 0}} &&
          fade.state() == State::fading_in, "fade in complement and exact deadline");
    effects.clear();
    fade.update(429, [&](Effect effect) {
        check(fade.state() == State::fading_in, "completion state is written after owner callbacks");
        record(effect);
    });
    check(effects == std::vector<Effect>{{alpha, 0}, {control, 1}} &&
          fade.state() == State::idle_clear, "fade in completion effect order");

    fade.event("FadeOut", 125, 500, record); fade.update(564, record);
    effects.clear();
    fade.event("FadeIn", 125, 564, record); fade.update(564, record);
    check(effects == std::vector<Effect>{{alpha, 254}}, "replacement starts from endpoint, not current alpha");
    fade.event("Unknown", 0xffffffffU, 0, {});
    effects.clear(); fade.update(628, record);
    check(effects == std::vector<Effect>{{alpha, 127}}, "unknown event preserves timing and state");

    effects.clear();
    for (const auto argument : {0xffffffffU, 0x80000000U, 0x7fffffffU})
        rejects([&] { fade.event("FadeOut", argument, 0, record); });
    rejects([&] { fade.event("FadeOut", 125, std::numeric_limits<std::int32_t>::max(), record); });
    rejects([&] { fade.event("FadeIn", 0, 0, {}); });
    rejects([&] { fade.update(563, record); });
    rejects([&] { fade.update(628, {}); });
    check(effects.empty() && fade.state() == State::fading_in, "invalid inputs fail before effects or state changes");

    fade.event("FadeOut", 1, -20, record);
    effects.clear(); fade.update(-19, record);
    check(effects == std::vector<Effect>{{alpha, 254}} && fade.state() == State::fading_out,
          "one-unit duration truncation and signed negative clock");
    fade.update(-18, record); check(fade.state() == State::idle_covered, "negative clock completion");
    fade.event("FadeOut", 84208, 0, record);
    effects.clear(); fade.update(86228, record); fade.update(86229, record);
    check(effects == std::vector<Effect>{{alpha, 253}, {alpha, 254}} &&
          fade.state() == State::fading_out, "binary32 duration chain differs from direct double arithmetic");
    fade.update(86230, record); check(fade.state() == State::idle_covered, "rounded duration completion");

    fade.event("FadeIn", 125, 1000, record);
    effects.clear();
    rejects([&] { fade.event("FadeOut", 125, 1100, [&](Effect effect) {
        record(effect);
        check(fade.state() == State::fading_in, "timed output callbacks see old state");
        if (effect.kind == alpha) throw std::runtime_error("visitor failure");
    }); });
    check(effects == std::vector<Effect>{{control, 0}, {alpha, 1}} &&
          fade.state() == State::fading_in, "event throw preserves output prefix, not uncommitted timing");
    effects.clear(); fade.update(1064, record);
    check(effects == std::vector<Effect>{{alpha, 127}}, "throw did not replace old transition timing");
    effects.clear();
    rejects([&] { fade.update(1129, [&](Effect effect) {
        record(effect);
        if (effect.kind == control) throw std::runtime_error("completion failure");
    }); });
    check(effects == std::vector<Effect>{{alpha, 0}, {control, 1}} &&
          fade.state() == State::fading_in, "completion throw precedes terminal state write");
    effects.clear(); fade.update(1129, record);
    check(effects == std::vector<Effect>{{alpha, 0}, {control, 1}} &&
          fade.state() == State::idle_clear, "guard releases and completion may be retried");

    effects.clear();
    fade.event("FadeOut", 0, 0, [&](Effect effect) {
        rejects([&] { fade.event("Unknown", 0, 0, {}); });
        rejects([&] { fade.update(0, record); });
        record(effect);
    });
    check(effects == std::vector<Effect>{{control, 0}, {alpha, 254}} &&
          fade.state() == State::idle_covered, "caught recursive calls cannot alter outer dispatch");
    rejects([&] { fade.event("FadeIn", 0, 0, [&](Effect) { fade.update(0, record); }); });
    check(fade.state() == State::idle_covered, "uncaught recursion prevents following state write");
    fade.event("FadeIn", 0, 0, record);
    check(fade.state() == State::idle_clear, "guard releases after recursive exception");

    std::vector<Effect> target_effects;
    FirstCutFade target{0x100000004ULL, 2, 4,
                        [&](bool enabled) { target_effects.push_back({control, static_cast<std::uint8_t>(enabled)}); },
                        [&](std::uint8_t value) { target_effects.push_back({alpha, value}); }};
    static_assert(!std::is_copy_constructible_v<FirstCutFade> && !std::is_move_constructible_v<FirstCutFade>);
    check(target.owner_identity() == 0x100000004ULL && target.fade_in_event() == 2 && target.fade_out_event() == 4 &&
          target.state() == State::idle_covered, "first-cut target retains admitted owner and concrete events only");
    target.event(9, 0xffffffffU, std::numeric_limits<std::int32_t>::max());
    check(target_effects.empty() && target.state() == State::idle_covered,
          "unknown typed event is inert without forwarding a synthetic name");
    target.event(4, 125, 100);
    check(target_effects == std::vector<Effect>{{control, 0}, {alpha, 1}},
          "typed fade out sends owner control then alpha");
    target_effects.clear(); target.update(229);
    check(target_effects == std::vector<Effect>{{alpha, 254}} && target.state() == State::idle_covered,
          "typed update delegates completion to picture fade");
    target.event(2, 0, 300);
    check(target_effects == std::vector<Effect>{{alpha, 254}, {control, 1}} && target.state() == State::idle_clear,
          "typed fade in sends owner control through injected sink");
    rejects([&] { FirstCutFade{0, 2, 4, [](bool) {}, [](std::uint8_t) {}}; });
    rejects([&] { FirstCutFade{1, 2, 2, [](bool) {}, [](std::uint8_t) {}}; });
    rejects([&] { FirstCutFade{1, 2, 4, {}, [](std::uint8_t) {}}; });
    return failures == 0 ? 0 : 1;
}
