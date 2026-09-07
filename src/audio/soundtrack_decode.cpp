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
#include <fstream>
#include <limits>
#include <memory>
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

[[nodiscard]] std::size_t checked_sample_values(unsigned channels,
                                                 unsigned sample_rate,
                                                 std::uint64_t frames) {
  if ((channels != 1U && channels != 2U) || sample_rate == 0U || frames == 0U ||
      frames > maximum_decoded_sample_values / channels)
    throw std::runtime_error("soundtrack stream has unsupported audio metadata");
  const auto values = frames * channels;
  if (values > std::numeric_limits<std::size_t>::max())
    throw std::runtime_error("decoded soundtrack exceeds the safety limit");
  return static_cast<std::size_t>(values);
}

[[nodiscard]] DecodedAudio decode_flac(std::span<const std::byte> bytes) {
  std::unique_ptr<drflac, decltype(&drflac_close)> decoder(
      drflac_open_memory(bytes.data(), bytes.size(), nullptr), drflac_close);
  if (!decoder)
    throw std::runtime_error("invalid FLAC soundtrack stream");
  const auto values = checked_sample_values(decoder->channels, decoder->sampleRate,
                                            decoder->totalPCMFrameCount);
  DecodedAudio result{.encoding = Encoding::flac,
                      .sample_rate = decoder->sampleRate,
                      .channels = decoder->channels,
                      .interleaved_samples = {}};
  result.interleaved_samples.resize(values);
  std::uint64_t complete = 0;
  while (complete < decoder->totalPCMFrameCount) {
    const auto request = std::min<std::uint64_t>(4'096U,
        decoder->totalPCMFrameCount - complete);
    const auto read = drflac_read_pcm_frames_s16(
        decoder.get(), request,
        result.interleaved_samples.data() + complete * result.channels);
    if (read != request)
      throw std::runtime_error("FLAC soundtrack stream ended early");
    complete += read;
  }
  return result;
}

[[nodiscard]] DecodedAudio decode_mp3(std::span<const std::byte> bytes) {
  drmp3 decoder{};
  if (!drmp3_init_memory(&decoder, bytes.data(), bytes.size(), nullptr))
    throw std::runtime_error("invalid MP3 soundtrack stream");
  struct Uninit final { drmp3 *value; ~Uninit() { drmp3_uninit(value); } } uninit{&decoder};
  const auto frames = drmp3_get_pcm_frame_count(&decoder);
  const auto values = checked_sample_values(decoder.channels, decoder.sampleRate, frames);
  DecodedAudio result{.encoding = Encoding::mp3,
                      .sample_rate = decoder.sampleRate,
                      .channels = decoder.channels,
                      .interleaved_samples = {}};
  result.interleaved_samples.resize(values);
  std::uint64_t complete = 0;
  while (complete < frames) {
    const auto request = std::min<std::uint64_t>(4'096U, frames - complete);
    const auto read = drmp3_read_pcm_frames_s16(
        &decoder, request,
        result.interleaved_samples.data() + complete * result.channels);
    if (read != request)
      throw std::runtime_error("MP3 soundtrack stream ended early");
    complete += read;
  }
  return result;
}

} // namespace

DecodedAudio decode_soundtrack_file(const std::filesystem::path& path) {
  const auto extension = lowercase_extension(path);
  if (extension != ".flac" && extension != ".mp3")
    throw std::runtime_error("soundtrack format must be FLAC or MP3");
  const auto bytes = read_soundtrack_file(path);
  return extension == ".flac" ? decode_flac(bytes) : decode_mp3(bytes);
}

} // namespace off::audio
