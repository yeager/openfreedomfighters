#include "off/audio/soundtrack_stream.hpp"

#include "dr_flac.h"
#include "dr_mp3.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace off::audio {
namespace {

constexpr std::uintmax_t maximum_encoded_audio_bytes = 64ULL * 1024ULL * 1024ULL;

std::string lower_extension(const std::filesystem::path& path) {
  auto extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return extension;
}

void validate_info(const SoundtrackStreamInfo& info) {
  if ((info.encoding != Encoding::flac && info.encoding != Encoding::mp3) ||
      (info.channels != 1U && info.channels != 2U) || info.sample_rate == 0U ||
      info.total_frames == 0U)
    throw std::runtime_error("soundtrack stream has unsupported audio metadata");
}

}  // namespace

struct SoundtrackStream::Impl {
  SoundtrackStreamInfo stream_info;
  drflac* flac{};
  drmp3 mp3{};
  bool mp3_open{};
  bool finished{};
  std::uint64_t frames_read{};

  ~Impl() {
    if (flac) drflac_close(flac);
    if (mp3_open) drmp3_uninit(&mp3);
  }
};

SoundtrackStream::SoundtrackStream(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}
SoundtrackStream::~SoundtrackStream() = default;
SoundtrackStream::SoundtrackStream(SoundtrackStream&&) noexcept = default;
SoundtrackStream& SoundtrackStream::operator=(SoundtrackStream&&) noexcept = default;

SoundtrackStream SoundtrackStream::open(const std::filesystem::path& path) {
  const auto extension = lower_extension(path);
  auto impl = std::make_unique<Impl>();
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size == 0U || size > maximum_encoded_audio_bytes)
    throw std::runtime_error("soundtrack file has an invalid size");
  if (extension == ".flac") {
#if defined(_WIN32)
    impl->flac = drflac_open_file_w(path.c_str(), nullptr);
#else
    const auto native_path = path.string();
    impl->flac = drflac_open_file(native_path.c_str(), nullptr);
#endif
    if (!impl->flac) throw std::runtime_error("invalid FLAC soundtrack stream");
    impl->stream_info = {Encoding::flac, impl->flac->sampleRate,
                         impl->flac->channels, impl->flac->totalPCMFrameCount};
  } else if (extension == ".mp3") {
#if defined(_WIN32)
    if (!drmp3_init_file_w(&impl->mp3, path.c_str(), nullptr))
#else
    const auto native_path = path.string();
    if (!drmp3_init_file(&impl->mp3, native_path.c_str(), nullptr))
#endif
      throw std::runtime_error("invalid MP3 soundtrack stream");
    impl->mp3_open = true;
    const auto frames = drmp3_get_pcm_frame_count(&impl->mp3);
    impl->stream_info = {Encoding::mp3, impl->mp3.sampleRate, impl->mp3.channels,
                         frames};
  } else {
    throw std::invalid_argument("soundtrack format must be FLAC or MP3");
  }
  validate_info(impl->stream_info);
  return SoundtrackStream(std::move(impl));
}

const SoundtrackStreamInfo& SoundtrackStream::info() const noexcept {
  return impl_->stream_info;
}

std::size_t SoundtrackStream::read_frames(std::span<std::int16_t> output) {
  const auto channels = impl_->stream_info.channels;
  if (output.empty() || output.size() % channels != 0U ||
      output.size() / channels > maximum_read_frames)
    throw std::invalid_argument("soundtrack stream read buffer is invalid");
  // Preserve the input-size contract even after EOF.  This prevents a caller
  // from using a completed stream to bypass the same bounded-buffer checks
  // that protect every decoding call.
  if (impl_->finished) return 0;
  const auto requested = output.size() / channels;
  const auto read = impl_->stream_info.encoding == Encoding::flac
      ? drflac_read_pcm_frames_s16(impl_->flac, requested, output.data())
      : drmp3_read_pcm_frames_s16(&impl_->mp3, requested, output.data());
  const auto remaining = impl_->stream_info.total_frames - impl_->frames_read;
  if (read > remaining)
    throw std::runtime_error("soundtrack stream exceeded its advertised frame count");
  impl_->frames_read += read;
  // A decoder may satisfy a request exactly on its final PCM frame.  Mark that
  // state here instead of waiting for a speculative extra read: the playback
  // transport needs to flush promptly after submitting the final buffer.
  if (impl_->frames_read == impl_->stream_info.total_frames) {
    impl_->finished = true;
  } else if (read < requested) {
    throw std::runtime_error("soundtrack stream ended before its advertised frame count");
  }
  return static_cast<std::size_t>(read);
}

bool SoundtrackStream::ended() const noexcept { return impl_->finished; }

}  // namespace off::audio
