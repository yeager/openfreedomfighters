#include "off/audio/decode.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char *message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
template <class Function> void rejects(Function operation, const char *message) {
  try { operation(); } catch (const std::runtime_error &) { return; }
  check(false, message);
}
} // namespace

int main(int argc, char **argv) {
  rejects([] { static_cast<void>(off::audio::decode_soundtrack_file("missing.flac")); },
          "missing FLAC is rejected");
  rejects([] { static_cast<void>(off::audio::decode_soundtrack_file("missing.mp3")); },
          "missing MP3 is rejected");
  rejects([] { static_cast<void>(off::audio::decode_soundtrack_file("not-a-track.wav")); },
          "non-soundtrack extension is rejected");
  for (int index = 1; index < argc; ++index) {
    const std::filesystem::path path{argv[index]};
    const auto decoded = off::audio::decode_soundtrack_file(path);
    check((path.extension() == ".flac" && decoded.encoding == off::audio::Encoding::flac) ||
              (path.extension() == ".mp3" && decoded.encoding == off::audio::Encoding::mp3),
          "owned soundtrack uses its declared decoder");
    check((decoded.channels == 1U || decoded.channels == 2U) &&
              decoded.sample_rate > 0U && decoded.frame_count() > 0U,
          "owned soundtrack produces supported PCM frames");
  }
  std::cout << "soundtrack decoder tests passed\n";
}
