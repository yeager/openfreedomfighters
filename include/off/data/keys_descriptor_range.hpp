#pragma once

#include "off/data/keys_property_materializer.hpp"
#include "off/data/matpos_owner_child_selector.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>

namespace off::data {

// The supported KEYS child contains an eight-byte child header followed by ten
// little-endian words. This is intentionally a descriptor boundary: it does
// not inspect, decode, or sample the separately owned key backing.
struct KeysDescriptorRange final {
    std::uint32_t count_like{};
    std::int32_t inclusive_first{};
    std::int32_t inclusive_last{};
    float spacing_or_rate{};
    std::array<std::uint32_t, 3> first_offsets{};
    std::array<std::uint32_t, 3> second_offsets{};
};

// Availability is explicitly supplied by the owner of the existing backing.
// Its request contains no backing pointer or samples, so this parser cannot
// accidentally turn descriptor admission into key evaluation.
struct KeysDescriptorBackingRequest final {
    std::uint64_t owner{};
    std::uint32_t buf_auxiliary_offset{};
    std::uint64_t opaque_handle{};
    KeysDescriptorRange descriptor{};
};

using KeysDescriptorBackingAvailability =
    std::function<bool(const KeysDescriptorBackingRequest&)>;

struct BoundKeysDescriptorRange final {
    std::uint64_t owner{};
    std::uint32_t buf_auxiliary_offset{};
    std::uint64_t opaque_handle{};
    KeysDescriptorRange descriptor{};
};

// Binds the recovered fixed descriptor only when an independent backing owner
// confirms that the existing key data is available. Absence is not repaired
// with allocation, guessed ranges, or a sampled fallback.
class KeysDescriptorRangeBinder final {
public:
    [[nodiscard]] static std::optional<BoundKeysDescriptorRange> bind(
        const MaterializedOwnerKeysChild& materialized,
        const OwnerAuxiliaryPropertyChild& child,
        const KeysDescriptorBackingAvailability& backing_available) {
        validate_identity(materialized, child);
        const auto descriptor = decode(child.bytes);
        validate_range(descriptor);

        if (!backing_available) {
            return std::nullopt;
        }
        const KeysDescriptorBackingRequest request{
            .owner = materialized.owner,
            .buf_auxiliary_offset = materialized.buf_auxiliary_offset,
            .opaque_handle = materialized.opaque_handle,
            .descriptor = descriptor,
        };
        if (!backing_available(request)) {
            return std::nullopt;
        }
        return BoundKeysDescriptorRange{
            .owner = materialized.owner,
            .buf_auxiliary_offset = materialized.buf_auxiliary_offset,
            .opaque_handle = materialized.opaque_handle,
            .descriptor = descriptor,
        };
    }

private:
    static constexpr std::size_t keys_extent = 48U;
    static constexpr std::size_t descriptor_word_offset = 8U;

    static void validate_identity(const MaterializedOwnerKeysChild& materialized,
                                  const OwnerAuxiliaryPropertyChild& child) {
        if (materialized.owner == 0U || materialized.buf_auxiliary_offset == 0U ||
            materialized.opaque_handle == 0U) {
            fail("KEYS descriptor requires a materialized live owner and backing handle");
        }
        if (child.buf_auxiliary_offset != materialized.buf_auxiliary_offset ||
            child.name != matpos_owner_child_selector || child.declared_extent != keys_extent ||
            child.bytes.size() != keys_extent) {
            fail("KEYS descriptor requires the supported materialized child identity");
        }
        if (read_u32(child.bytes, 0U) != matpos_owner_child_selector_word ||
            read_u32(child.bytes, sizeof(std::uint32_t)) != keys_extent) {
            fail("KEYS descriptor requires its fixed child header");
        }
    }

    [[nodiscard]] static KeysDescriptorRange decode(std::span<const std::byte> bytes) noexcept {
        const auto word = [&bytes](std::size_t index) {
            return read_u32(bytes, descriptor_word_offset + index * sizeof(std::uint32_t));
        };
        return KeysDescriptorRange{
            .count_like = word(0U),
            .inclusive_first = static_cast<std::int32_t>(word(1U)),
            .inclusive_last = static_cast<std::int32_t>(word(2U)),
            .spacing_or_rate = std::bit_cast<float>(word(3U)),
            .first_offsets = {word(4U), word(5U), word(6U)},
            .second_offsets = {word(7U), word(8U), word(9U)},
        };
    }

    static void validate_range(const KeysDescriptorRange& descriptor) {
        if (descriptor.count_like == 0U || !std::isfinite(descriptor.spacing_or_rate)) {
            fail("KEYS descriptor requires a finite nonempty range contract");
        }
        if (descriptor.inclusive_first > descriptor.inclusive_last) {
            fail("KEYS descriptor requires ordered signed inclusive bounds");
        }
        const auto span = static_cast<std::uint64_t>(
                              static_cast<std::int64_t>(descriptor.inclusive_last) -
                              static_cast<std::int64_t>(descriptor.inclusive_first)) +
                          1U;
        if (span > descriptor.count_like) {
            fail("KEYS descriptor inclusive range exceeds its declared count");
        }
    }

    [[nodiscard]] static std::uint32_t read_u32(std::span<const std::byte> bytes,
                                                std::size_t offset) noexcept {
        return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
               (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U])) << 8U) |
               (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2U])) << 16U) |
               (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3U])) << 24U);
    }

    [[noreturn]] static void fail(const char* message) { throw std::runtime_error(message); }
};

} // namespace off::data
