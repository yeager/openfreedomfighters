#pragma once

#include "off/audio/decode.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>

namespace off::audio {

struct SoundtrackStreamInfo {
  Encoding encoding{};
  std::uint32_t sample_rate{};
  std::uint32_t channels{};
  // Exact decoded PCM frames reported by the container. This is metadata only:
  // it does not decode, cache, map, or schedule the album track.
  std::uint64_t total_frames{};
};

// Sequential, bounded decoder for a verified optional soundtrack file. It has
// no cue knowledge and owns no audio device. The caller supplies a buffer for
// at most maximum_read_frames frames and receives only actual decoded frames.
class SoundtrackStream final {
 public:
  static constexpr std::size_t maximum_read_frames = 65'536;

  [[nodiscard]] static SoundtrackStream open(const std::filesystem::path& path);
  ~SoundtrackStream();
  SoundtrackStream(SoundtrackStream&&) noexcept;
  SoundtrackStream& operator=(SoundtrackStream&&) noexcept;
  SoundtrackStream(const SoundtrackStream&) = delete;
  SoundtrackStream& operator=(const SoundtrackStream&) = delete;

  [[nodiscard]] const SoundtrackStreamInfo& info() const noexcept;
  [[nodiscard]] std::size_t read_frames(std::span<std::int16_t> output);
  [[nodiscard]] bool ended() const noexcept;

 private:
  struct Impl;
  explicit SoundtrackStream(std::unique_ptr<Impl> impl) noexcept;
  std::unique_ptr<Impl> impl_;
};

}  // namespace off::audio
