#include "off/audio/sound_preferences.hpp"
#include "off/graphics/intro_controller_initialization.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Backend final : off::audio::SoundVolumeBackend {
  std::vector<std::int32_t> volumes;
  void request_category_volume(std::uint32_t category, std::int32_t volume, std::uint32_t mode) override {
    check(category == 0 && mode == 2, "SFX backend category and nonlinear mode request");
    volumes.push_back(volume);
  }
};
}
int main() {
  try {
    using namespace off::audio;
    SoundTextConfiguration application;
    Backend first, second;
    SoundVolumeBackend* live = &first;
    unsigned queries = 0;
    SoundPreferences* preferences = nullptr;
    bool mutate_on_query = false, fail_query = false;
    SoundPreferences sound({application, [&]() -> SoundVolumeBackend* {
      ++queries;
      check(preferences->configuration().find("SoundEffectsVolume") &&
            application.find("SoundEffectsVolume") &&
            *preferences->configuration().find("SoundEffectsVolume") == *application.find("SoundEffectsVolume"),
            "both configuration writes precede live backend query");
      if (fail_query) throw std::runtime_error("backend lookup failure");
      if (mutate_on_query) preferences->initialize_from_parsed_setting(23);
      return live;
    }});
    preferences = &sound;
    check(sound.volume() == 90, "constructor default");
    sound.configuration().erase("SoundEffectsVolume");
    application.erase("SoundEffectsVolume");
    sound.set_volume(90);
    check(queries == 0 && sound.configuration().deleted("SoundEffectsVolume") && application.deleted("SoundEffectsVolume"), "equal setter has no effects");
    for (auto value : {std::numeric_limits<std::int32_t>::min(), -1, 0, 37, 100, 101, std::numeric_limits<std::int32_t>::max()}) {
      sound.initialize_from_parsed_setting(value);
      check(sound.volume() == (value < 0 ? 0 : value > 100 ? 100 : value), "parsed initializer clamp");
    }
    sound.initialize_from_parsed_setting(std::nullopt);
    check(sound.volume() == 90 && queries == 0, "initializer reset without backend/config writes");
    sound.set_volume(37);
    check(first.volumes == std::vector<std::int32_t>{37} && *application.find("SoundEffectsVolume") == "37" && !application.deleted("SoundEffectsVolume") && !sound.configuration().deleted("SoundEffectsVolume"), "changed setter clears tombstones and updates backend");
    sound.set_volume(37);
    check(queries == 1, "equal live setting does not query backend");
    live = &second;
    sound.set_volume(UINT32_MAX);
    check(sound.volume() == -1 && second.volumes == std::vector<std::int32_t>{-1} && *application.find("SoundEffectsVolume") == "-1", "unclamped signed word and live backend replacement");
    live = nullptr;
    sound.set_volume(0x80000000U);
    check(sound.volume() == std::numeric_limits<std::int32_t>::min() && *application.find("SoundEffectsVolume") == "-2147483648", "signed minimum formats without overflow; absent backend allowed");
    live = &second; mutate_on_query = true;
    sound.set_volume(50);
    check(second.volumes.back() == 23 && *application.find("SoundEffectsVolume") == "50", "backend receives then-current retained volume");
    mutate_on_query = false; fail_query = true;
    bool failed = false;
    try { sound.set_volume(55); } catch (const std::runtime_error&) { failed = true; }
    check(failed && sound.volume() == 55 && *application.find("SoundEffectsVolume") == "55", "failure preserves ordered prefix");
    fail_query = false;
    off::graphics::IntroControllerPhaseTwoServices services;
    unsigned non_audio_calls = 0;
    services.scene_integer_clock = [&] { ++non_audio_calls; return 123U; };
    sound.bind_phase_two_services(services);
    check(services.current_audio_volume() == 55 && services.scene_integer_clock() == 123 && non_audio_calls == 1, "binding preserves non-audio services");
    services.request_audio_volume(63);
    check(sound.volume() == 63 && second.volumes.back() == 63, "binding updates canonical preferences");
    bool missing_rejected = false;
    try { SoundPreferences invalid({application, {}}); } catch (const std::invalid_argument&) { missing_rejected = true; }
    check(missing_rejected, "missing resolver is not an absent backend");
    std::cout << "Sound preferences tests passed\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n'; return 1;
  }
}
