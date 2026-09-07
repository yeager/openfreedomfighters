#include "off/audio/decode.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma clang diagnostic ignored "-Wstring-conversion"
#endif
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <algorithm>
#include <cctype>
#include <functional>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace off::audio {
namespace {

constexpr std::uintmax_t maximum_encoded_audio_bytes = 64ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t maximum_decoded_sample_values = 64ULL * 1024ULL * 1024ULL;

[[nodiscard]] std::vector<std::byte> read_soundtrack_file(
    const std::filesystem::path& path) {
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size == 0 || size > maximum_encoded_audio_bytes ||
      size > std::numeric_limits<std::size_t>::max())
    throw std::runtime_error("soundtrack file has an invalid size");
  std::ifstream input(path, std::ios::binary);
  if (!input)
    throw std::runtime_error("soundtrack file could not be opened");
  std::vector<std::byte> result(static_cast<std::size_t>(size));
  input.read(reinterpret_cast<char*>(result.data()),
             static_cast<std::streamsize>(result.size()));
  if (!input || input.gcount() != static_cast<std::streamsize>(result.size()))
    throw std::runtime_error("soundtrack file could not be read completely");
  return result;
}

[[nodiscard]] std::string lowercase_extension(const std::filesystem::path& path) {
  auto extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
  return extension;
}

[[nodiscard]] DecodedAudio make_decoded(Encoding encoding, unsigned channels,
                                        unsigned sample_rate, std::uint64_t frames,
                                        std::int16_t* samples,
                                        const std::function<void(void*)>& release) {
  if (samples == nullptr || (channels != 1U && channels != 2U) || sample_rate == 0U ||
      frames == 0U || frames > maximum_decoded_sample_values / channels) {
    if (samples != nullptr)
      release(samples);
    throw std::runtime_error("soundtrack stream has unsupported audio metadata");
  }
  const auto values = frames * channels;
  if (values > std::numeric_limits<std::size_t>::max()) {
    release(samples);
    throw std::runtime_error("decoded soundtrack exceeds the safety limit");
  }
  DecodedAudio result{.encoding = encoding,
                      .sample_rate = sample_rate,
                      .channels = channels,
                      .interleaved_samples = {}};
  result.interleaved_samples.assign(samples, samples + static_cast<std::size_t>(values));
  release(samples);
  return result;
}

} // namespace

DecodedAudio decode_soundtrack_file(const std::filesystem::path& path) {
  const auto extension = lowercase_extension(path);
  if (extension != ".flac" && extension != ".mp3")
    throw std::runtime_error("soundtrack format must be FLAC or MP3");
  const auto bytes = read_soundtrack_file(path);
  if (extension == ".flac") {
    unsigned channels = 0;
    unsigned sample_rate = 0;
    drflac_uint64 frames = 0;
    auto* samples = drflac_open_memory_and_read_pcm_frames_s16(
        bytes.data(), bytes.size(), &channels, &sample_rate, &frames, nullptr);
    return make_decoded(Encoding::flac, channels, sample_rate, frames, samples,
                        [](void* memory) { drflac_free(memory, nullptr); });
  }
  if (extension == ".mp3") {
    drmp3_config configuration{};
    drmp3_uint64 frames = 0;
    auto* samples = drmp3_open_memory_and_read_pcm_frames_s16(
        bytes.data(), bytes.size(), &configuration, &frames, nullptr);
    return make_decoded(Encoding::mp3, configuration.channels,
                        configuration.sampleRate, frames, samples,
                        [](void* memory) { drmp3_free(memory, nullptr); });
  }
  throw std::runtime_error("unreachable soundtrack decoder selection");
}

} // namespace off::audio
