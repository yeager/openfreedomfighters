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
    vorbis,
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

// Selected bank record decoding: validates the WHD meaningful 16-bit output
// counts separately from physical codec length. PCM/Vorbis must match exactly;
// IMA may contain final-block padding, which is removed only after proving that
// enough real decoded samples exist. No silence, timing or readiness is invented.
// decode_stream remains available for independent physical-codec inspection.
[[nodiscard]] DecodedAudio decode_bank_stream(
    const data::AudioStreamRecord& record,
    std::span<const std::byte> encoded
);

}  // namespace off::audio
