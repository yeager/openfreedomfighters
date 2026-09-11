#include "off/audio/soundtrack_stream.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
template <class Function> void rejects(Function operation, const char* message) {
  try { operation(); } catch (const std::exception&) { return; }
  check(false, message);
}
}  // namespace

int main(int argc, char** argv) {
  rejects([] { static_cast<void>(off::audio::SoundtrackStream::open("missing.flac")); },
          "missing FLAC is rejected");
  rejects([] { static_cast<void>(off::audio::SoundtrackStream::open("missing.mp3")); },
          "missing MP3 is rejected");
  rejects([] { static_cast<void>(off::audio::SoundtrackStream::open("not-a-track.ogg")); },
          "unsupported format is rejected");
  for (int index = 1; index < argc; ++index) {
    auto stream = off::audio::SoundtrackStream::open(argv[index]);
    const auto info = stream.info();
    check((info.encoding == off::audio::Encoding::flac || info.encoding == off::audio::Encoding::mp3) &&
              (info.channels == 1U || info.channels == 2U) && info.sample_rate > 0U &&
              info.total_frames > 0U,
          "owned soundtrack has supported metadata");
    std::vector<std::int16_t> buffer(info.channels * 257U);
    const auto frames = stream.read_frames(buffer);
    check(frames > 0U && frames <= 257U, "bounded read returns actual PCM frames");
    std::vector<std::int16_t> oversized(
        (off::audio::SoundtrackStream::maximum_read_frames + 1U) * info.channels);
    rejects([&] { static_cast<void>(stream.read_frames(oversized)); },
        "oversized read is rejected");
  }
  std::cout << "soundtrack stream tests passed\n";
}
