#pragma once

#include "off/data/keys_descriptor_range.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace off::data {

// An immutable snapshot of the complete scene-owned FF-Intro BUF allocation.
// A generation is deliberately carried separately from descriptor handles: a
// descriptor never implies that an old scene allocation is still live.
class ImmutableKeysBackingView final {
public:
    [[nodiscard]] static std::optional<ImmutableKeysBackingView> create(
        std::uint64_t generation, std::span<const std::byte> complete_buf) {
        if (generation == 0U || complete_buf.empty()) {
            return std::nullopt;
        }
        return ImmutableKeysBackingView(
            generation, std::make_shared<const std::vector<std::byte>>(
                            complete_buf.begin(), complete_buf.end()));
    }

    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept { return *bytes_; }

private:
    ImmutableKeysBackingView(std::uint64_t generation,
                             std::shared_ptr<const std::vector<std::byte>> bytes) noexcept
        : generation_(generation), bytes_(std::move(bytes)) {}

    std::uint64_t generation_{};
    std::shared_ptr<const std::vector<std::byte>> bytes_;
};

// These component groups are intentionally anonymous. Their transform roles
// have not been recovered, so the evaluator only returns numeric samples.
struct KeysBackingSample final {
    std::array<float, 4> first_group{};
    std::array<float, 3> second_group{};
};

// Read-only parser for the FF-Intro profile proven for the six descriptor
// offsets. It is not a phase-one or playback service.
class KeysBackingEvaluator final {
public:
    [[nodiscard]] static std::optional<KeysBackingSample> evaluate(
        const ImmutableKeysBackingView& backing,
        const BoundKeysDescriptorRange& bound,
        float normalized_coordinate) noexcept {
        if (backing.generation() == 0U || backing.bytes().empty() ||
            bound.owner == 0U || bound.buf_auxiliary_offset == 0U ||
            bound.opaque_handle == 0U || bound.descriptor.count_like == 0U ||
            !std::isfinite(normalized_coordinate) || normalized_coordinate < 0.0F ||
            normalized_coordinate > 1.0F) {
            return std::nullopt;
        }

        const auto last = static_cast<std::uint64_t>(bound.descriptor.count_like - 1U);
        const auto position = static_cast<double>(normalized_coordinate) *
                              static_cast<double>(last);
        if (!std::isfinite(position)) {
            return std::nullopt;
        }
        const auto current = static_cast<std::uint64_t>(std::floor(position));
        if (current > last) {
            return std::nullopt;
        }
        const auto interpolate = current < last;
        const auto next = interpolate ? current + 1U : current;
        const auto fraction = interpolate ? static_cast<float>(position - static_cast<double>(current)) : 0.0F;
        if (!std::isfinite(fraction) || fraction < 0.0F || fraction > 1.0F) {
            return std::nullopt;
        }

        const auto first_current = first_group(backing.bytes(), bound.descriptor.first_offsets, current);
        const auto first_next = first_group(backing.bytes(), bound.descriptor.first_offsets, next);
        const auto second_current = second_group(backing.bytes(), bound.descriptor.second_offsets, current);
        const auto second_next = second_group(backing.bytes(), bound.descriptor.second_offsets, next);
        if (!first_current || !first_next || !second_current || !second_next) {
            return std::nullopt;
        }
        const KeysBackingSample result{
            .first_group = blend(*first_current, *first_next, fraction),
            .second_group = blend(*second_current, *second_next, fraction),
        };
        return finite(result.first_group) && finite(result.second_group) ? std::optional(result) : std::nullopt;
    }

private:
    [[nodiscard]] static bool contains(std::span<const std::byte> bytes, std::uint64_t offset,
                                       std::uint64_t extent) noexcept {
        return offset <= bytes.size() && extent <= bytes.size() - offset;
    }

    [[nodiscard]] static std::optional<std::uint32_t> packed_index(
        std::span<const std::byte> bytes, std::uint32_t stream_offset,
        std::uint64_t entry) noexcept {
        if (!contains(bytes, stream_offset, 1U)) {
            return std::nullopt;
        }
        const auto width = std::to_integer<std::uint8_t>(bytes[stream_offset]);
        if (width == 0U || width > 24U || entry > std::numeric_limits<std::uint64_t>::max() / width) {
            return std::nullopt;
        }
        const auto bit_offset = entry * width;
        if (bit_offset / 8U > std::numeric_limits<std::uint64_t>::max() -
                                 static_cast<std::uint64_t>(stream_offset) - 1U) {
            return std::nullopt;
        }
        const auto byte_offset = static_cast<std::uint64_t>(stream_offset) + 1U + bit_offset / 8U;
        // The recovered reader fetches a three-byte MSB-first window at every
        // position, including when the requested value itself is shorter.
        if (!contains(bytes, byte_offset, 3U)) {
            return std::nullopt;
        }
        const auto window = (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[byte_offset])) << 16U) |
                            (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[byte_offset + 1U])) << 8U) |
                            static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[byte_offset + 2U]));
        const auto intra_byte = static_cast<unsigned int>(bit_offset % 8U);
        return (window >> (24U - intra_byte - width)) & ((std::uint32_t{1} << width) - 1U);
    }

    [[nodiscard]] static std::optional<std::uint32_t> final_index(
        std::span<const std::byte> bytes, const std::array<std::uint32_t, 3>& offsets,
        std::uint64_t sample) noexcept {
        const auto first = packed_index(bytes, offsets[0], sample);
        if (!first) {
            return std::nullopt;
        }
        if (*first == 0U) {
            return 0U;
        }
        return packed_index(bytes, offsets[1], static_cast<std::uint64_t>(*first) - 1U);
    }

    [[nodiscard]] static std::optional<std::array<float, 4>> first_group(
        std::span<const std::byte> bytes, const std::array<std::uint32_t, 3>& offsets,
        std::uint64_t sample) noexcept {
        const auto index = final_index(bytes, offsets, sample);
        if (!index || *index > (std::numeric_limits<std::uint64_t>::max() - offsets[2]) / 8U) {
            return std::nullopt;
        }
        const auto base = static_cast<std::uint64_t>(offsets[2]) + static_cast<std::uint64_t>(*index) * 8U;
        if (!contains(bytes, base, 8U)) {
            return std::nullopt;
        }
        std::array<float, 4> result{};
        for (std::size_t i = 0; i < result.size(); ++i) {
            const auto low = std::to_integer<std::uint8_t>(bytes[base + i * 2U]);
            const auto high = std::to_integer<std::uint8_t>(bytes[base + i * 2U + 1U]);
            const std::uint16_t bits = static_cast<std::uint16_t>(low) |
                                       (static_cast<std::uint16_t>(high) << 8U);
            result[i] = static_cast<float>(std::bit_cast<std::int16_t>(bits));
        }
        return result;
    }

    [[nodiscard]] static std::optional<std::array<float, 3>> second_group(
        std::span<const std::byte> bytes, const std::array<std::uint32_t, 3>& offsets,
        std::uint64_t sample) noexcept {
        const auto index = final_index(bytes, offsets, sample);
        if (!index || *index > (std::numeric_limits<std::uint64_t>::max() - offsets[2]) / 12U) {
            return std::nullopt;
        }
        const auto base = static_cast<std::uint64_t>(offsets[2]) + static_cast<std::uint64_t>(*index) * 12U;
        if (!contains(bytes, base, 12U)) {
            return std::nullopt;
        }
        std::array<float, 3> result{};
        for (std::size_t i = 0; i < result.size(); ++i) {
            const auto byte = base + i * 4U;
            const auto bits = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[byte])) |
                              (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[byte + 1U])) << 8U) |
                              (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[byte + 2U])) << 16U) |
                              (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[byte + 3U])) << 24U);
            result[i] = std::bit_cast<float>(bits);
            if (!std::isfinite(result[i])) {
                return std::nullopt;
            }
        }
        return result;
    }

    template <std::size_t N>
    [[nodiscard]] static std::array<float, N> blend(const std::array<float, N>& current,
                                                     const std::array<float, N>& next,
                                                     float fraction) noexcept {
        std::array<float, N> result{};
        for (std::size_t i = 0; i < N; ++i) {
            result[i] = current[i] + (next[i] - current[i]) * fraction;
        }
        return result;
    }

    template <std::size_t N>
    [[nodiscard]] static bool finite(const std::array<float, N>& values) noexcept {
        for (const auto value : values) {
            if (!std::isfinite(value)) {
                return false;
            }
        }
        return true;
    }
};

} // namespace off::data
