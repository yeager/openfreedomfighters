#include "off/audio/soundtrack_playback.hpp"
#include "off/crypto/sha256.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
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
    player.start({off::audio::SoundtrackFormat::flac, "missing.flac", std::string(64U, 'a')});
    check(false, "missing source must reject");
  } catch (const std::runtime_error&) {}
  check(opened == 0U && !player.status(), "decoder failure does not admit output");
  player.pump();
  player.pause();
  player.resume();
  player.stop();
  check(!player.status(), "idle control calls are harmless");
  const auto replacement = std::filesystem::current_path() /
      "OFF - Soundtrack - 01 Playback-Replacement.flac";
  {
    std::ofstream output(replacement, std::ios::binary | std::ios::trunc);
    output << "not a soundtrack stream";
  }
  struct ReplacementCleanup final {
    std::filesystem::path path;
    ~ReplacementCleanup() { std::error_code error; std::filesystem::remove(path, error); }
  } replacement_cleanup{replacement};
  std::size_t replacement_output_opens{};
  off::audio::SoundtrackPlayback replacement_player(
      [&](std::uint32_t, std::size_t) {
        ++replacement_output_opens;
        return std::make_unique<UnusedOutput>();
      });
  try {
    replacement_player.start({off::audio::SoundtrackFormat::flac, replacement,
                              std::string(64U, 'b')});
    check(false, "replaced soundtrack must reject");
  } catch (const std::runtime_error&) {}
  check(replacement_output_opens == 0U && !replacement_player.status(),
        "changed soundtrack never reaches the output boundary");
  if (argc > 1) {
    auto capture = std::make_shared<Capture>();
    off::audio::SoundtrackPlayback playback(
        [capture](std::uint32_t rate, std::size_t bytes)
            -> std::unique_ptr<off::audio::StereoPcmOutput> {
          check(rate > 0U && bytes == 4096U, "source format and bounded queue are passed");
          return std::make_unique<RecordingOutput>(capture);
        }, {1024U, 256U});
    const auto digest = off::crypto::to_hex(off::crypto::sha256_file(argv[1]));
    playback.start({off::audio::SoundtrackFormat::flac, argv[1], digest});
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
