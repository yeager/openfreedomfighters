#include "off/audio/decode.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <stdexcept>

namespace off::audio {
namespace {

constexpr std::uint32_t global_bank_flag = 0x80000000U;
constexpr std::uint32_t pcm_format = 0x00000001U;
constexpr std::uint32_t ima_adpcm_format = 0x00000011U;
constexpr std::uint64_t maximum_decoded_sample_values = 64ULL * 1024ULL * 1024ULL;

constexpr std::array<int, 16> index_adjustment{
    -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8,
};

constexpr std::array<int, 89> step_table{
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37,
    41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173,
    190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1'060, 1'166, 1'282, 1'411, 1'552, 1'707, 1'878,
    2'066, 2'272, 2'499, 2'749, 3'024, 3'327, 3'660, 4'026, 4'428, 4'871,
    5'358, 5'894, 6'484, 7'132, 7'845, 8'630, 9'493, 10'442, 11'487,
    12'635, 13'899, 15'289, 16'818, 18'500, 20'350, 22'385, 24'623, 27'086,
    29'794, 32'767,
};

struct ImaState {
    int predictor{0};
    int step_index{0};
};

[[nodiscard]] std::uint16_t little_u16(const std::byte* bytes) noexcept {
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint16_t>(bytes[0]) |
        (std::to_integer<std::uint16_t>(bytes[1]) << 8U)
    );
}

[[nodiscard]] std::int16_t little_i16(const std::byte* bytes) noexcept {
    return std::bit_cast<std::int16_t>(little_u16(bytes));
}

[[nodiscard]] std::int16_t decode_nibble(ImaState& state, std::uint8_t nibble) {
    const auto step = step_table[static_cast<std::size_t>(state.step_index)];
    auto difference = step >> 3;
    if ((nibble & 1U) != 0) {
        difference += step >> 2;
    }
    if ((nibble & 2U) != 0) {
        difference += step >> 1;
    }
    if ((nibble & 4U) != 0) {
        difference += step;
    }
    state.predictor += (nibble & 8U) != 0 ? -difference : difference;
    state.predictor = std::clamp(state.predictor, -32'768, 32'767);
    state.step_index = std::clamp(
        state.step_index + index_adjustment[static_cast<std::size_t>(nibble)],
        0,
        88
    );
    return static_cast<std::int16_t>(state.predictor);
}

[[nodiscard]] ImaState read_ima_state(const std::byte* header) {
    const auto index = std::to_integer<unsigned int>(header[2]);
    if (index > 88 || header[3] != std::byte{0}) {
        throw std::runtime_error("invalid IMA ADPCM block header");
    }
    return {
        .predictor = little_i16(header),
        .step_index = static_cast<int>(index),
    };
}

void decode_mono_block(
    std::span<const std::byte> block,
    std::vector<std::int16_t>& output
) {
    auto state = read_ima_state(block.data());
    output.push_back(static_cast<std::int16_t>(state.predictor));
    for (const auto byte : block.subspan(4)) {
        const auto value = std::to_integer<std::uint8_t>(byte);
        output.push_back(decode_nibble(state, value & 0x0fU));
        output.push_back(decode_nibble(state, value >> 4U));
    }
}

void decode_stereo_block(
    std::span<const std::byte> block,
    std::vector<std::int16_t>& output
) {
    auto left = read_ima_state(block.data());
    auto right = read_ima_state(block.data() + 4);
    output.push_back(static_cast<std::int16_t>(left.predictor));
    output.push_back(static_cast<std::int16_t>(right.predictor));

    std::array<std::int16_t, 8> left_samples{};
    std::array<std::int16_t, 8> right_samples{};
    for (std::size_t group = 8; group < block.size(); group += 8) {
        for (std::size_t byte_index = 0; byte_index < 4; ++byte_index) {
            const auto left_byte = std::to_integer<std::uint8_t>(block[group + byte_index]);
            left_samples[byte_index * 2] = decode_nibble(left, left_byte & 0x0fU);
            left_samples[byte_index * 2 + 1] = decode_nibble(left, left_byte >> 4U);
            const auto right_byte = std::to_integer<std::uint8_t>(block[group + 4 + byte_index]);
            right_samples[byte_index * 2] = decode_nibble(right, right_byte & 0x0fU);
            right_samples[byte_index * 2 + 1] = decode_nibble(right, right_byte >> 4U);
        }
        for (std::size_t sample = 0; sample < left_samples.size(); ++sample) {
            output.push_back(left_samples[sample]);
            output.push_back(right_samples[sample]);
        }
    }
}

[[nodiscard]] DecodedAudio decode_pcm(
    const data::AudioStreamRecord& record,
    std::span<const std::byte> encoded
) {
    if (record.bits_per_sample != 16 || (record.channels != 1 && record.channels != 2) ||
        record.block_align != record.channels * 2 || record.samples_per_block != 1 ||
        encoded.size() % record.block_align != 0) {
        throw std::runtime_error("unsupported PCM stream layout");
    }
    const auto sample_values = encoded.size() / 2;
    if (sample_values > maximum_decoded_sample_values) {
        throw std::runtime_error("decoded PCM stream exceeds the safety limit");
    }
    DecodedAudio result{
        .encoding = Encoding::pcm_s16le,
        .sample_rate = record.sample_rate,
        .channels = record.channels,
        .interleaved_samples = {},
    };
    result.interleaved_samples.reserve(sample_values);
    for (std::size_t offset = 0; offset < encoded.size(); offset += 2) {
        result.interleaved_samples.push_back(little_i16(encoded.data() + offset));
    }
    return result;
}

[[nodiscard]] DecodedAudio decode_ima_adpcm(
    const data::AudioStreamRecord& record,
    std::span<const std::byte> encoded
) {
    if (record.bits_per_sample != 4 || (record.channels != 1 && record.channels != 2) ||
        record.block_align < record.channels * 4 || encoded.size() % record.block_align != 0) {
        throw std::runtime_error("unsupported IMA ADPCM stream layout");
    }
    const auto header_bytes = record.channels * 4;
    const auto data_bytes = record.block_align - header_bytes;
    if ((record.channels == 2 && data_bytes % 8 != 0) ||
        (record.channels == 1 && data_bytes > (std::numeric_limits<std::uint32_t>::max() - 1) / 2)) {
        throw std::runtime_error("invalid IMA ADPCM block alignment");
    }
    const auto calculated_samples_per_block = record.channels == 1
        ? 1U + data_bytes * 2U
        : 1U + data_bytes;
    if (record.samples_per_block != calculated_samples_per_block) {
        throw std::runtime_error("IMA ADPCM samples-per-block value is inconsistent");
    }
    const auto block_count = encoded.size() / record.block_align;
    const auto sample_values = static_cast<std::uint64_t>(block_count) *
        record.samples_per_block * record.channels;
    if (sample_values > maximum_decoded_sample_values ||
        sample_values > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("decoded IMA ADPCM stream exceeds the safety limit");
    }

    DecodedAudio result{
        .encoding = Encoding::ima_adpcm,
        .sample_rate = record.sample_rate,
        .channels = record.channels,
        .interleaved_samples = {},
    };
    result.interleaved_samples.reserve(static_cast<std::size_t>(sample_values));
    for (std::size_t offset = 0; offset < encoded.size(); offset += record.block_align) {
        const auto block = encoded.subspan(offset, record.block_align);
        if (record.channels == 1) {
            decode_mono_block(block, result.interleaved_samples);
        } else {
            decode_stereo_block(block, result.interleaved_samples);
        }
    }
    return result;
}

}  // namespace

DecodedAudio decode_stream(
    const data::AudioStreamRecord& record,
    std::span<const std::byte> encoded
) {
    if (encoded.size() != record.encoded_size) {
        throw std::runtime_error("encoded audio size does not match its WHD record");
    }
    switch (record.format_flags & ~global_bank_flag) {
        case pcm_format:
            return decode_pcm(record, encoded);
        case ima_adpcm_format:
            return decode_ima_adpcm(record, encoded);
        default:
            throw std::runtime_error("unsupported audio encoding");
    }
}

}  // namespace off::audio
