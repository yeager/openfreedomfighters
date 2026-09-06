#pragma once
#include "off/data/audio_bank_header.hpp"
#include "off/data/vfs_file_view.hpp"
#include <memory>
#include <string>
#include <vector>

namespace off::audio {
enum class IntroAudioStreamState { idle, initial_pending, initial_completed, ready,
                                  pcm_pending, pcm_completed, ended, failed, cancelled };
struct IntroAudioPcm {
  std::vector<std::int16_t> interleaved_samples;
  bool end_of_stream{};
};
// Native sequential Vorbis stream policy, not channel admission or playback.
// Single controlling thread. Worker I/O/decoding owns no sound-record reference.
// Control methods do not wait for worker I/O. They may allocate or create a
// thread and are not real-time safe. Destruction/move assignment join the worker.
// Short sources read their actual encoded length, never padded windows. Cancel
// is terminal; descriptors are never reused, so late completion cannot retarget.
// Subsequent worker refills are sequential native policy, not original sizing.
class IntroAudioStream final {
public:
  static constexpr std::size_t window_bytes=32768;
  static constexpr std::size_t maximum_request_frames=65536;
  IntroAudioStream(data::AudioStreamRecord record, data::VfsFileReader reader);
  ~IntroAudioStream();
  IntroAudioStream(IntroAudioStream&&) noexcept;
  IntroAudioStream& operator=(IntroAudioStream&&) noexcept;
  IntroAudioStream(const IntroAudioStream&)=delete;
  IntroAudioStream& operator=(const IntroAudioStream&)=delete;
  void issue_initial_read();
  // Poll only transfers worker completion; initial readiness needs observation.
  [[nodiscard]] IntroAudioStreamState poll();
  void observe_initial_completion();
  void request_pcm(std::size_t frames);
  [[nodiscard]] IntroAudioPcm take_pcm();
  void cancel() noexcept;
  [[nodiscard]] IntroAudioStreamState state() const noexcept;
  [[nodiscard]] std::string error() const;
  [[nodiscard]] std::size_t encoded_bytes_read() const noexcept;
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace off::audio
