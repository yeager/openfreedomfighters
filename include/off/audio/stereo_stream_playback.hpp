#pragma once
#include "off/audio/intro_audio_stream.hpp"
#include "off/audio/stereo_pcm_output.hpp"
#include "off/audio/stereo_source_command.hpp"
#include <functional>

namespace off::audio {
struct StereoPlaybackNotifications {
  // Serialize all starts before all stops. Stopping never erases a pending start.
  std::vector<std::uint64_t> started, stopped;
};
struct StereoStreamStatus {
  bool started{}, held{}, input_ended{};
  std::uint64_t submitted_frames{};
  std::size_t queued_input_bytes{}, encoded_bytes_read{};
};
struct StereoStreamPlaybackLimits {
  // Native bounded buffering/allocation policies, not original pool dimensions.
  std::size_t channels{65}, queue_frames{16384}, decode_frames{4096};
};
// Consumes already admitted actual nonloop, ungrouped stereo Vorbis commands.
// This is the channel service, not listener/SGP admission or the complete batch
// assembler. Its production output factory must create real device outputs.
// One controlling thread; no methods are real-time safe. Stop/destruction join
// owned decoder workers. Callers must keep factory captures alive until teardown.
class StereoStreamPlayback final {
public:
  using SourceResolver=std::function<IntroAudioStreamSource(std::uint32_t whd_link)>;
  using OutputFactory=std::function<std::unique_ptr<StereoPcmOutput>(
      std::uint32_t sample_rate,std::size_t queue_bytes)>;
  StereoStreamPlayback(SourceResolver source,OutputFactory output,
                       StereoStreamPlaybackLimits limits={});
  ~StereoStreamPlayback();
  StereoStreamPlayback(const StereoStreamPlayback&)=delete;
  StereoStreamPlayback& operator=(const StereoStreamPlayback&)=delete;
  void submit(std::span<const StereoSourceCommand> commands);
  // Caller applies the two ordered stop lists before the following sources.
  void stop(std::uint64_t binding);
  void hold(std::uint64_t binding,bool held);
  // Nonblocking worker polls and bounded refill; starts only after real PCM
  // submission. Device errors retire the affected channel and propagate.
  void pump();
  [[nodiscard]] std::optional<StereoStreamStatus> status(std::uint64_t binding) const;
  [[nodiscard]] StereoPlaybackNotifications take_notifications();
private:
  struct Channel;
  SourceResolver source_;
  OutputFactory output_;
  StereoStreamPlaybackLimits limits_;
  std::vector<std::unique_ptr<Channel>> channels_;
  StereoPlaybackNotifications notifications_;
  mutable bool busy_{};
  [[nodiscard]] std::unique_ptr<Channel>* find(std::uint64_t binding);
  void update(Channel& channel,const StereoSourceCommand& command);
  void refill(Channel& channel);
};
} // namespace off::audio
