#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace off::graphics { struct IntroControllerPhaseTwoServices; }

namespace off::audio {

// Retained text configuration, not a disk writer or numeric scene property store.
class SoundTextConfiguration final {
public:
  void set(std::string_view key, std::string value);
  void erase(std::string_view key);
  [[nodiscard]] const std::string* find(std::string_view key) const;
  [[nodiscard]] bool deleted(std::string_view key) const;
private:
  std::map<std::string, std::string, std::less<>> settings_;
  std::set<std::string, std::less<>> deleted_;
};

class SoundVolumeBackend {
public:
  virtual ~SoundVolumeBackend() = default;
  virtual void request_category_volume(std::uint32_t category,
                                       std::int32_t volume,
                                       std::uint32_t mode) = 0;
};

struct ApplicationSoundServices {
  SoundTextConfiguration& configuration;
  // Required resolver; returning nullptr explicitly means no live backend.
  std::function<SoundVolumeBackend*()> live_backend;
};

class SoundPreferences final {
public:
  explicit SoundPreferences(ApplicationSoundServices services);
  SoundPreferences(const SoundPreferences&) = delete;
  SoundPreferences& operator=(const SoundPreferences&) = delete;
  // Parsing and duplicate-key policy belong to the application configuration
  // loader. This initializer consumes its already resolved signed value only.
  void initialize_from_parsed_setting(std::optional<std::int32_t> value) noexcept;
  [[nodiscard]] std::int32_t volume() const noexcept { return volume_; }
  // Bit-pattern comparison and signed decimal formatting; deliberately no clamp.
  // Failures preserve completed state/configuration effects, with no rollback.
  void set_volume(std::uint32_t word);
  [[nodiscard]] SoundTextConfiguration& configuration() noexcept { return configuration_; }
  [[nodiscard]] const SoundTextConfiguration& configuration() const noexcept { return configuration_; }
  // Only replaces audio callbacks. This object and borrowed application state
  // must outlive all bound calls; resolver-returned backend must survive its call.
  void bind_phase_two_services(graphics::IntroControllerPhaseTwoServices& services);
private:
  ApplicationSoundServices application_;
  SoundTextConfiguration configuration_;
  std::int32_t volume_{90};
};
} // namespace off::audio
