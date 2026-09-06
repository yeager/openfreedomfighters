#include "off/audio/sound_preferences.hpp"
#include "off/graphics/intro_controller_initialization.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include <utility>

namespace off::audio {
void SoundTextConfiguration::set(std::string_view key, std::string value) {
  const std::string owned_key(key);
  deleted_.erase(owned_key);
  settings_.insert_or_assign(owned_key, std::move(value));
}
void SoundTextConfiguration::erase(std::string_view key) {
  const std::string owned_key(key);
  settings_.erase(owned_key);
  deleted_.insert(owned_key);
}
const std::string* SoundTextConfiguration::find(std::string_view key) const {
  const auto found = settings_.find(key);
  return found == settings_.end() ? nullptr : &found->second;
}
bool SoundTextConfiguration::deleted(std::string_view key) const {
  return deleted_.contains(key);
}
SoundPreferences::SoundPreferences(ApplicationSoundServices services)
  : application_(std::move(services)) {
  if (!application_.live_backend) throw std::invalid_argument("Missing live sound backend resolver");
}
void SoundPreferences::initialize_from_parsed_setting(std::optional<std::int32_t> value) noexcept {
  volume_ = value ? std::clamp(*value, std::int32_t{0}, std::int32_t{100}) : 90;
}
void SoundPreferences::set_volume(std::uint32_t word) {
  if (word == std::bit_cast<std::uint32_t>(volume_)) return;
  volume_ = std::bit_cast<std::int32_t>(word);
  const auto text = std::to_string(volume_);
  configuration_.set("SoundEffectsVolume", text);
  application_.configuration.set("SoundEffectsVolume", text);
  if (auto* backend = application_.live_backend()) {
    backend->request_category_volume(0, volume_, 2);
  }
}
void SoundPreferences::bind_phase_two_services(graphics::IntroControllerPhaseTwoServices& services) {
  services.current_audio_volume = [this] { return volume(); };
  services.request_audio_volume = [this](std::uint32_t word) { set_volume(word); };
}
} // namespace off::audio
