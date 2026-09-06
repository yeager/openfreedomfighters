#pragma once
#include "off/audio/sound_preferences.hpp"
#include "off/data/gms_image.hpp"
#include "off/data/sound_definition_bank.hpp"
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace off::audio {
struct SoundCategory { float gain{}; bool selected{}; std::int32_t adjustment{}; };
struct SoundRecord {
  std::uint64_t binding{}, owner{}, parent{};
  std::array<std::uint32_t,3> backend_list_tokens{};
  std::optional<std::array<float,3>> position;
  std::array<float,3> direction{0,0,1}, previous_direction{0,0,1};
  std::uint32_t playback_state{3}, type{1}, output_mode{2}, flags{0x86}, category{};
  std::optional<std::uint32_t> active_source;
  std::uint32_t alternate_source{}, start_time{};
  float duration{}, progress{}, seek{}, gain{1}, gain_multiplier{1};
  std::array<float,4> source_scalars{360,360,0,0};
  float range{10000}, derived_range{12800};
  std::optional<float> final_scalar;
  std::array<std::int32_t,2> timing_changes{};
  std::uint32_t source_mode{};
  std::int32_t pan{}, grouping_count{};
  std::optional<std::int32_t> priority, environment_group_index;
  std::optional<bool> fade_enabled;
  std::optional<std::array<float,3>> fade_values;
};
class SoundRecordRegistry;
class SoundRecordLease final {
public:
  SoundRecordLease() = default;
  ~SoundRecordLease();
  SoundRecordLease(SoundRecordLease&&) noexcept;
  SoundRecordLease& operator=(SoundRecordLease&&) noexcept;
  SoundRecordLease(const SoundRecordLease&) = delete;
  SoundRecordLease& operator=(const SoundRecordLease&) = delete;
  [[nodiscard]] std::uint64_t binding() const noexcept { return binding_; }
  [[nodiscard]] SoundRecord& get() const;
  void reset() noexcept;
private:
  friend class SoundRecordRegistry;
  SoundRecordLease(SoundRecordRegistry& registry,std::uint64_t binding):registry_(&registry),binding_(binding) {}
  SoundRecordRegistry* registry_{};
  std::uint64_t binding_{};
};
// Native logical backend; no output-device or playback-start producer is implied.
// Must outlive every lease. Single-threaded/external synchronization required.
class SoundRecordRegistry final : public SoundVolumeBackend {
public:
  static constexpr std::size_t capacity=1024;
  static constexpr std::size_t category_count=4;
  SoundRecordRegistry();
  SoundRecordRegistry(const SoundRecordRegistry&) = delete;
  SoundRecordRegistry& operator=(const SoundRecordRegistry&) = delete;
  [[nodiscard]] SoundRecordLease create(std::uint64_t owner);
  [[nodiscard]] SoundRecord* resolve(std::uint64_t binding) noexcept;
  [[nodiscard]] const SoundRecord* resolve(std::uint64_t binding) const noexcept;
  [[nodiscard]] std::size_t size() const noexcept { return records_.size(); }
  void apply_source(SoundRecord& record,const data::GmsIntroSoundOwnerPrefix& source);
  [[nodiscard]] bool prepare(std::uint64_t binding,const data::SoundDefinitionBank& bank,std::uint32_t raw_time);
  void stop(std::uint64_t binding);
  // Receiver only. Parsing/decoding/preparation must never manufacture this call.
  void acknowledge_started(std::uint64_t binding,std::uint32_t raw_time);
  [[nodiscard]] std::array<SoundCategory,8>& categories() noexcept { return categories_; }
  [[nodiscard]] const std::array<SoundCategory,8>& categories() const noexcept { return categories_; }
  [[nodiscard]] std::span<const std::uint64_t> prepared() const noexcept { return prepared_; }
  [[nodiscard]] std::span<const std::uint64_t> pending_stops() const noexcept { return pending_stops_; }
  // One retained mode gates both preparation's extra timing branch and the
  // category-volume state-5 traversal. Preparation while true is unsupported.
  void set_special_mode(bool value) noexcept { special_mode_=value; }
  [[nodiscard]] bool pending_volume_update() const noexcept { return pending_volume_update_; }
  // Mode 0 is linear; mode 2 uses the integer response curve. Neither clamps.
  // These mutate retained state, not device initialization or playback ACKs.
  void request_category_volume(std::uint32_t category,std::int32_t volume,std::uint32_t mode) override;
  [[nodiscard]] float master_gain() const noexcept { return master_gain_; }
  void set_master_volume(std::int32_t percent) noexcept;
private:
  friend class SoundRecordLease;
  void release(std::uint64_t binding) noexcept;
  std::map<std::uint64_t,std::unique_ptr<SoundRecord>> records_;
  std::uint64_t next_binding_{1};
  std::array<SoundCategory,8> categories_{};
  std::vector<std::uint64_t> prepared_,pending_stops_;
  bool special_mode_{},pending_volume_update_{};
  float master_gain_{1};
};
} // namespace off::audio
