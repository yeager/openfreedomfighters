#include "off/audio/decode.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

off::data::AudioStreamRecord record(
    std::uint32_t flags,
    std::uint32_t encoded_size,
    std::uint32_t channels,
    std::uint32_t block_align,
    std::uint32_t samples_per_block
) {
    return {
        .format_flags = flags,
        .sample_rate = 22'050,
        .bits_per_sample = flags == 1 ? 16U : 4U,
        .encoded_size = encoded_size,
        .channels = channels,
        .block_align = block_align,
        .samples_per_block = samples_per_block,
    };
}

}  // namespace

int main() {
    const std::array pcm_bytes{
        std::byte{0x00}, std::byte{0x80}, std::byte{0xff}, std::byte{0xff},
        std::byte{0x00}, std::byte{0x00}, std::byte{0xff}, std::byte{0x7f},
    };
    const auto pcm = off::audio::decode_stream(record(1, 8, 1, 2, 1), pcm_bytes);
    check(
        pcm.interleaved_samples == std::vector<std::int16_t>{-32'768, -1, 0, 32'767},
        "decode signed little-endian PCM"
    );
    check(pcm.frame_count() == 4, "report decoded PCM frames");

    const std::array mono_block{
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x11}, std::byte{0x11}, std::byte{0x11}, std::byte{0x11},
    };
    const auto mono = off::audio::decode_stream(record(0x80000011U, 8, 1, 8, 9), mono_block);
    check(
        mono.interleaved_samples == std::vector<std::int16_t>{0, 1, 2, 3, 4, 5, 6, 7, 8},
        "decode low-nibble-first mono IMA ADPCM"
    );

    const std::array stereo_block{
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{100}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0x11}, std::byte{0x11}, std::byte{0x11}, std::byte{0x11},
        std::byte{0x99}, std::byte{0x99}, std::byte{0x99}, std::byte{0x99},
    };
    const auto stereo = off::audio::decode_stream(record(0x11, 16, 2, 16, 9), stereo_block);
    check(stereo.frame_count() == 9, "report decoded stereo IMA ADPCM frames");
    check(
        stereo.interleaved_samples == std::vector<std::int16_t>{
            0, 100, 1, 99, 2, 98, 3, 97, 4, 96, 5, 95, 6, 94, 7, 93, 8, 92,
        },
        "interleave stereo IMA ADPCM channel groups"
    );

    auto invalid_index = mono_block;
    invalid_index[2] = std::byte{89};
    bool index_rejected = false;
    try {
        static_cast<void>(off::audio::decode_stream(
            record(0x80000011U, 8, 1, 8, 9),
            invalid_index
        ));
    } catch (const std::runtime_error&) {
        index_rejected = true;
    }
    check(index_rejected, "reject an invalid IMA ADPCM step index");

    bool layout_rejected = false;
    try {
        static_cast<void>(off::audio::decode_stream(record(0x11, 8, 1, 8, 8), mono_block));
    } catch (const std::runtime_error&) {
        layout_rejected = true;
    }
    check(layout_rejected, "reject inconsistent IMA ADPCM block metadata");

    bool zero_channels_rejected = false;
    try {
        static_cast<void>(off::audio::decode_stream(record(1, 8, 0, 0, 1), pcm_bytes));
    } catch (const std::runtime_error&) {
        zero_channels_rejected = true;
    }
    check(zero_channels_rejected, "reject zero-channel PCM without division by zero");

    bool unknown_rejected = false;
    try {
        static_cast<void>(off::audio::decode_stream(record(0x80001000U, 8, 2, 4, 1), pcm_bytes));
    } catch (const std::runtime_error&) {
        unknown_rejected = true;
    }
    check(unknown_rejected, "reject an unverified encoding family");

    return failures == 0 ? 0 : 1;
}
