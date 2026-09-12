#pragma once

#include "off/data/keys_property_materializer.hpp"
#include "off/data/matpos_owner_child_selector.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace off::data {

// The recovered MatPos owner-property profile is deliberately narrower than a
// BUF grammar. It accepts exactly the one observed 64-byte outer record and
// exposes its one bounded KEYS child for KeysPropertyMaterializer. Other BUF
// forms must be recovered independently rather than added as fallbacks here.
class OwnerBufKeysProfileParser final {
public:
    [[nodiscard]] static OwnerAuxiliaryPropertyChild parse(
        const OwnerAuxiliaryPropertyBlock& property) {
        constexpr std::size_t outer_extent = 64U;
        constexpr std::size_t child_offset = 16U;
        constexpr std::size_t child_extent = 48U;

        if (property.owner == 0U || property.buf_auxiliary_offset == 0U ||
            property.bytes.size() != outer_extent) {
            fail("owner BUF KEYS profile requires an exact live 64-byte property block");
        }

        const auto words = property.bytes;
        if (read_u32(words, 0U) != 0U) {
            fail("owner BUF KEYS profile requires a zero first word");
        }
        const auto tagged_extent = read_u32(words, 4U);
        if ((tagged_extent & 0xc0000000U) != 0x80000000U ||
            (tagged_extent & 0x3fffffffU) != outer_extent) {
            fail("owner BUF KEYS profile requires its observed tagged outer extent");
        }
        if (read_u32(words, 8U) != outer_extent || read_u32(words, 12U) != 1U) {
            fail("owner BUF KEYS profile requires its observed outer header");
        }
        if (read_u32(words, child_offset) != matpos_owner_child_selector_word ||
            read_u32(words, child_offset + sizeof(std::uint32_t)) != child_extent) {
            fail("owner BUF KEYS profile requires its exact inclusive KEYS child");
        }

        return OwnerAuxiliaryPropertyChild{
            .buf_auxiliary_offset = property.buf_auxiliary_offset,
            .name = matpos_owner_child_selector,
            .declared_extent = child_extent,
            .bytes = property.bytes.subspan(child_offset, child_extent),
        };
    }

private:
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
