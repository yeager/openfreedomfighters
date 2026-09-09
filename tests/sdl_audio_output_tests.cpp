#include "off/platform/sdl_audio_output.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace {
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
template<class F> void rejects(F&& operation) {
  bool rejected = false;
  try { operation(); } catch (const std::exception&) { rejected = true; }
  require(rejected, "expected explicit rejection");
}
template<class F> void rejects_without_unavailable(F&& operation) {
  bool rejected = false;
  try { operation(); }
  catch (const off::platform::SdlAudioUnavailable&) {
    throw std::runtime_error("invalid input/backend was classified as unavailable");
  }
  catch (const std::exception&) { rejected = true; }
  require(rejected, "expected non-availability rejection");
}
}

int main(int argc, char** argv) {
  try {
    using Output = off::platform::SdlAudioOutput;
    require(Output::linear_gain_from_volume(-10000) == 0.0F,
            "minimum volume must map to silence");
    require(Output::linear_gain_from_volume(0) == 1.0F,
            "zero dB must map to unity");
    require(std::abs(Output::linear_gain_from_volume(-2000) - 0.1F) < 1.0e-7F &&
            std::abs(Output::linear_gain_from_volume(-4000) - 0.01F) < 1.0e-8F &&
            Output::linear_gain_from_volume(-9999) > 0.0F,
            "native logarithmic amplitude mapping");
    rejects_without_unavailable([] { (void)Output::linear_gain_from_volume(-10001); });
    rejects_without_unavailable([] { (void)Output::linear_gain_from_volume(1); });
    require(Output::frequency_ratio_for(100) == 0.01F,
            "minimum legacy frequency must map to SDL's supported minimum ratio");
    require(std::abs(Output::frequency_ratio_for(44100) - 4.41F) < 1.0e-6F,
            "44.1 kHz legacy frequency mapping");
    require(Output::frequency_ratio_for(100000) == 10.0F,
            "maximum legacy frequency mapping");
    rejects_without_unavailable([] { (void)Output::frequency_ratio_for(99); });
    rejects_without_unavailable([] { (void)Output::frequency_ratio_for(100001); });
    rejects_without_unavailable([] { off::platform::SdlAudioOutput output(0); });
    rejects_without_unavailable([] { off::platform::SdlAudioOutput output(44100, 3); });
    if (argc == 2 && std::string_view(argv[1]) == "--reject-dummy") {
      require(SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"), "set synthetic rejection driver");
      rejects_without_unavailable([] { off::platform::SdlAudioOutput output(44100); });
      std::cout << "Synthetic dummy driver rejected; no device proof claimed.\n";
      return 0;
    }
    std::unique_ptr<off::platform::SdlAudioOutput> output;
    try { output = std::make_unique<off::platform::SdlAudioOutput>(44100, 17640); }
    catch (const off::platform::SdlAudioUnavailable& error) {
      std::cout << "SKIP: real playback device unavailable: " << error.what() << '\n';
      return 77;
    }
    std::cout << "Opened real SDL driver: " << output->driver() << std::endl;
    require(!output->capabilities().spatial_effects, "spatial capability must be absent");
    rejects([&] { output->start(); });
    rejects([&] { output->set_gain(std::numeric_limits<float>::quiet_NaN()); });
    rejects([&] { output->set_gain(-1.0F); });
    rejects([&] { output->set_frequency(99); });
    output->set_frequency(100);
    output->set_frequency(100000);
    rejects([&] { output->set_pan(1); });
    output->set_pan(0);
    off::audio::StereoPcmOutput& channel_output = *output;
    rejects([&] { channel_output.set_volume_hundredths_db(1); });
    rejects([&] { channel_output.set_volume_hundredths_db(-10001); });
    channel_output.set_volume_hundredths_db(0); // Generated silent PCM only.
    output->set_frequency(44100);
    const std::array<std::int16_t, 1> partial{0};
    rejects([&] { output->submit(partial); });
    const std::vector<std::int16_t> pcm(8820, 0);
    output->submit(pcm);
    require(output->queued_input_bytes() == 17640, "initial paused PCM queue");
    const std::array<std::int16_t, 2> frame{0, 0};
    rejects([&] { output->submit(frame); });
    output->flush();
    output->start();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (output->queued_input_bytes() != 0 && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (output->queued_input_bytes() != 0)
      std::cerr << "Remaining input PCM bytes: " << output->queued_input_bytes() << '\n';
    require(output->queued_input_bytes() == 0, "real device did not consume PCM");
    output->pause();
    output->submit(pcm);
    {
      off::platform::SdlAudioOutput other(48000, 19200);
      other.submit(frame);
      other.flush();
      other.start();
      other.stop();
      require(output->queued_input_bytes() == 17640, "independent logical device pause");
    }
    output->start();
    output->stop();
    require(output->queued_input_bytes() == 0, "stop must clear queue");
    rejects([&] { output->start(); });
    std::cout << "Real SDL driver " << output->driver()
              << " consumed synthetic silent PCM; no audibility claim.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
