#include "off/audio/soundtrack_playback.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
class UnusedOutput final : public off::audio::StereoPcmOutput {
 public:
  void submit(std::span<const std::int16_t>) override {}
  std::size_t queued_input_bytes() const override { return 0; }
  void start() override {}
  void pause() override {}
  void stop() override {}
  void flush() override {}
  void set_volume_hundredths_db(std::int32_t) override {}
  void set_frequency(std::uint32_t) override {}
  void set_pan(std::int32_t) override {}
};
struct Capture {
  std::size_t queued{};
  std::size_t submitted{};
  std::size_t starts{};
  std::size_t flushes{};
  std::uint32_t frequency{};
  std::int32_t pan{1};
  std::int32_t volume{1};
};
class RecordingOutput final : public off::audio::StereoPcmOutput {
 public:
  explicit RecordingOutput(std::shared_ptr<Capture> capture) : capture_(std::move(capture)) {}
  void submit(std::span<const std::int16_t> samples) override {
    capture_->submitted += samples.size() / 2U;
    capture_->queued += samples.size_bytes();
  }
  std::size_t queued_input_bytes() const override { return capture_->queued; }
  void start() override { ++capture_->starts; }
  void pause() override {}
  void stop() override { capture_->queued = 0; }
  void flush() override { ++capture_->flushes; }
  void set_volume_hundredths_db(std::int32_t value) override { capture_->volume = value; }
  void set_frequency(std::uint32_t value) override { capture_->frequency = value; }
  void set_pan(std::int32_t value) override { capture_->pan = value; }
 private:
  std::shared_ptr<Capture> capture_;
};
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
}  // namespace

int main(int argc, char** argv) {
  std::size_t opened{};
  off::audio::SoundtrackPlayback player(
      [&](std::uint32_t, std::size_t) {
        ++opened;
        return std::make_unique<UnusedOutput>();
      });
  try {
    player.start({off::audio::SoundtrackFormat::flac, "missing.flac"});
    check(false, "missing source must reject");
  } catch (const std::runtime_error&) {}
  check(opened == 0U && !player.status(), "decoder failure does not admit output");
  player.pump();
  player.pause();
  player.resume();
  player.stop();
  check(!player.status(), "idle control calls are harmless");
  if (argc > 1) {
    auto capture = std::make_shared<Capture>();
    off::audio::SoundtrackPlayback playback(
        [capture](std::uint32_t rate, std::size_t bytes)
            -> std::unique_ptr<off::audio::StereoPcmOutput> {
          check(rate > 0U && bytes == 4096U, "source format and bounded queue are passed");
          return std::make_unique<RecordingOutput>(capture);
        }, {1024U, 256U});
    playback.start({off::audio::SoundtrackFormat::flac, argv[1]});
    check(capture->frequency > 0U && capture->pan == 0 && capture->volume == 0,
          "output controls are applied before PCM");
    playback.pump();
    const auto status = playback.status();
    check(status && status->started && status->submitted_frames > 0U &&
              capture->starts == 1U && capture->submitted == status->submitted_frames,
          "PCM submission precedes exactly one playback start");
    check(status->queued_input_bytes <= 4096U, "transport preserves output queue bound");
    playback.stop();
    check(!playback.status() && capture->queued == 0U, "explicit stop clears the transport");
  }
  std::cout << "soundtrack playback tests passed\n";
}
