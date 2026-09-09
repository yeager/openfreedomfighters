#pragma once

#include "off/audio/soundtrack_catalog.hpp"
#include "off/audio/stereo_pcm_output.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace off::audio {

struct SoundtrackPlaybackLimits {
  std::size_t queue_frames{16'384};
  std::size_t decode_frames{4'096};
};

struct SoundtrackPlaybackStatus {
  bool started{};
  bool input_ended{};
  std::uint64_t submitted_frames{};
  std::size_t queued_input_bytes{};
};

// Pumps one optional album edition into a real stereo PCM output. Callers may
// only select an edition after independently established game-cue timing and
// loop behavior; this type intentionally has no game-cue interface.
class SoundtrackPlayback final {
 public:
  using OutputFactory = std::function<std::unique_ptr<StereoPcmOutput>(
      std::uint32_t sample_rate, std::size_t queue_bytes)>;

  explicit SoundtrackPlayback(OutputFactory output,
                             SoundtrackPlaybackLimits limits = {});
  ~SoundtrackPlayback();
  SoundtrackPlayback(const SoundtrackPlayback&) = delete;
  SoundtrackPlayback& operator=(const SoundtrackPlayback&) = delete;

  // The caller must supply an edition obtained from a verified catalog.
  void start(const SoundtrackEdition& edition);
  void pump();
  void pause();
  void resume();
  void stop();
  [[nodiscard]] std::optional<SoundtrackPlaybackStatus> status() const;

 private:
  struct Active;
  OutputFactory output_;
  SoundtrackPlaybackLimits limits_;
  std::unique_ptr<Active> active_;
};

}  // namespace off::audio
