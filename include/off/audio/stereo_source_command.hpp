#pragma once
#include "off/audio/sound_records.hpp"
#include <string>

namespace off::audio {
struct StereoSourceCommand {
  std::uint64_t binding;
  bool start_requested, loop;
  std::int32_t priority;
  std::uint32_t whd_offset;
  std::int32_t environment_group, pan, frequency_adjustment;
  float gain;
  // This semantic type implies nonspatial channel mode 3/control scalar zero.
};
struct StereoCommandBatch {
  std::vector<StereoSourceCommand> sources;
  std::vector<std::string> diagnostics;
  std::size_t visited{};
};
// Stores the actual type-6/output-2 priority; does not sort, group or admit it.
[[nodiscard]] float weight_intro_stereo_source(SoundRecord& record);
// Input must be the real producer's already admitted priority order. No implicit
// preparation/listener/grouping/device admission. Visits at most 65 entries.
// Failure preserves earlier state-5 -> state-1 mutations; no rollback or ACK.
[[nodiscard]] StereoCommandBatch build_intro_stereo_commands(SoundRecordRegistry& registry,
    const data::SoundDefinitionBank& bank,std::span<const std::uint64_t> ordered);
struct StereoOutputControls {
  float retained_gain;
  std::int32_t pan, volume_hundredths_db;
  std::uint32_t frequency_hz;
};
// Pure conversion, not buffer effects or playback. Native transcendental math
// and explicit rejection of unrepresentable signed conversion are policies.
[[nodiscard]] StereoOutputControls stereo_output_controls(float gain,std::int32_t pan,
    std::int32_t frequency_adjustment,std::uint32_t sample_rate);
} // namespace off::audio
