#include "off/audio/soundtrack_playback.hpp"

#include "off/audio/soundtrack_stream.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace off::audio {

struct SoundtrackPlayback::Active {
  SoundtrackStream stream;
  std::unique_ptr<StereoPcmOutput> output;
  std::vector<std::int16_t> decoded;
  std::vector<std::int16_t> stereo;
  bool started{};
  bool paused{};
  bool input_ended{};
  std::uint64_t submitted_frames{};

  Active(SoundtrackStream source, std::unique_ptr<StereoPcmOutput> device,
         std::size_t decode_frames)
      : stream(std::move(source)), output(std::move(device)),
        decoded(decode_frames * stream.info().channels), stereo(decode_frames * 2U) {}
};

SoundtrackPlayback::SoundtrackPlayback(OutputFactory output,
                                       SoundtrackPlaybackLimits limits)
    : output_(std::move(output)), limits_(limits) {
  if (!output_ || limits_.decode_frames == 0U ||
      limits_.decode_frames > SoundtrackStream::maximum_read_frames ||
      limits_.queue_frames < limits_.decode_frames || limits_.queue_frames > 262'144U)
    throw std::invalid_argument("invalid soundtrack playback services or limits");
}

SoundtrackPlayback::~SoundtrackPlayback() = default;

void SoundtrackPlayback::start(const SoundtrackEdition& edition) {
  if (active_) throw std::runtime_error("soundtrack playback is already active");
  auto stream = SoundtrackStream::open(edition.path);
  const auto& info = stream.info();
  if (info.sample_rate < 100U || info.sample_rate > 100'000U)
    throw std::runtime_error("soundtrack sample rate is unsupported by the output");
  const auto queue_bytes = limits_.queue_frames * 4U;
  auto output = output_(info.sample_rate, queue_bytes);
  if (!output) throw std::runtime_error("soundtrack output factory returned no device");
  output->set_pan(0);
  output->set_volume_hundredths_db(0);
  output->set_frequency(info.sample_rate);
  active_ = std::make_unique<Active>(std::move(stream), std::move(output), limits_.decode_frames);
}

void SoundtrackPlayback::pump() {
  if (!active_ || active_->input_ended || active_->paused) return;
  auto& active = *active_;
  try {
    const auto queued = active.output->queued_input_bytes();
    const auto capacity = limits_.queue_frames * 4U;
    if (queued > capacity) throw std::runtime_error("soundtrack output exceeds its queue limit");
    const auto writable_frames = (capacity - queued) / 4U;
    if (writable_frames < limits_.decode_frames) return;
    const auto frames = active.stream.read_frames(active.decoded);
    if (frames != 0U) {
      std::span<const std::int16_t> pcm;
      if (active.stream.info().channels == 2U) {
        pcm = std::span(active.decoded).first(frames * 2U);
      } else {
        for (std::size_t frame = 0; frame < frames; ++frame) {
          active.stereo[frame * 2U] = active.decoded[frame];
          active.stereo[frame * 2U + 1U] = active.decoded[frame];
        }
        pcm = std::span(active.stereo).first(frames * 2U);
      }
      active.output->submit(pcm);
      active.submitted_frames += frames;
      if (!active.started) {
        active.output->start();
        active.started = true;
      }
    }
    if (active.stream.ended()) {
      active.output->flush();
      active.input_ended = true;
    }
  } catch (...) {
    active_.reset();
    throw;
  }
}

void SoundtrackPlayback::pause() {
  if (!active_ || active_->paused) return;
  try {
    if (active_->started) active_->output->pause();
    active_->paused = true;
  } catch (...) { active_.reset(); throw; }
}

void SoundtrackPlayback::resume() {
  if (!active_ || !active_->paused) return;
  try {
    if (active_->started) active_->output->start();
    active_->paused = false;
  } catch (...) { active_.reset(); throw; }
}

void SoundtrackPlayback::stop() {
  if (!active_) return;
  try { active_->output->stop(); } catch (...) { active_.reset(); throw; }
  active_.reset();
}

std::optional<SoundtrackPlaybackStatus> SoundtrackPlayback::status() const {
  if (!active_) return std::nullopt;
  return SoundtrackPlaybackStatus{active_->started, active_->input_ended,
      active_->submitted_frames, active_->output->queued_input_bytes()};
}

}  // namespace off::audio
