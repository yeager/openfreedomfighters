#pragma once

#include "off/data/audio_bank_header.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace off::audio {

enum class Encoding {
    pcm_s16le,
    ima_adpcm,
};

struct DecodedAudio {
    Encoding encoding{Encoding::pcm_s16le};
    std::uint32_t sample_rate{0};
    std::uint32_t channels{0};
    std::vector<std::int16_t> interleaved_samples;

    [[nodiscard]] std::size_t frame_count() const noexcept {
        return channels == 0 ? 0 : interleaved_samples.size() / channels;
    }
};

[[nodiscard]] DecodedAudio decode_stream(
    const data::AudioStreamRecord& record,
    std::span<const std::byte> encoded
);

}  // namespace off::audio
